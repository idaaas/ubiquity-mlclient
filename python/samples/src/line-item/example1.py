import time
import random
import itertools
from datetime import datetime, timedelta
import json

from Ubiquity import Ubiquity
from src.utils import apiUtils
from src.utils import unzip as u
from src.utils import data as d

# Constants
POLL_TIMEOUT = 0.2  # seconds
THROUGHPUT_IN_QPS = 300
PACE_IN_NANOS = int(1e9 / THROUGHPUT_IN_QPS)


model_schema = """
{{
    "cluster_id": "{id}",
    "name": "lineitem/Example1",
    "feedback_delays": [
    20,
    30,
    40
    ],
    "features": [
    {{
        "name": "app.bundle",
        "type": "nominal"
    }},
    {{
        "name": "imp.banner.pos",
        "type": "categorical"
    }},
    {{
        "name": "localtime",
        "type": "categorical"
    }},
    {{
        "name": "device.os",
        "type": "nominal"
    }},
    {{
        "name": "device.geo.city",
        "type": "nominal"
    }},
    {{
        "name": "device.model",
        "type": "nominal"
    }},
    {{
        "name": "bcat",
        "type": "nominal"
    }},
    {{
        "name": "device.geo.zip",
        "type": "nominal"
    }},
    {{
        "name": "user.yob",
        "type": "nominal"
    }},
    {{
        "name": "app.publisher.name",
        "type": "nominal"
    }},
    {{
        "name": "device.connectiontype",
        "type": "categorical"
    }},
    {{
        "name": "device.geo.region",
        "type": "nominal"
    }},
    {{
        "name": "app.cat",
        "type": "nominal"
    }},
    {{
        "name": "device.make",
        "type": "nominal"
    }},
    {{
        "name": "device.HxW",
        "type": "nominal"
    }},
    {{
        "name": "user.gender",
        "type": "categorical"
    }},
    {{
        "name": "device.language",
        "type": "nominal"
    }},
    {{
        "name": "imp.displaymanager",
        "type": "nominal"
    }},
    {{
        "name": "imp.banner.battr",
        "type": "nominal"
    }},
    {{
        "name": "device.geo.country",
        "type": "nominal"
    }},
    {{
        "name": "app.content.language",
        "type": "nominal"
    }},
    {{
        "name": "app.content.context",
        "type": "categorical"
    }},
    {{
        "name": "imp.banner.topframe",
        "type": "boolean"
    }},
    {{
        "name": "device.devicetype",
        "type": "categorical"
    }},
    {{
        "name": "imp.instl",
        "type": "boolean"
    }},
    {{
        "name": "app.pagecat",
        "type": "nominal"
    }}
    ],
"creatives": {{
    "features": [
    {{
        "name": "a0",
        "type": "numeric"
    }},
    {{
        "name": "a1",
        "type": "nominal"
    }},
    {{
        "name": "a2",
        "type": "boolean"
    }},
    {{
        "name": "a3",
        "type": "categorical"
    }}
    ],
    "values": [
    {{
        "id": 5,
        "features": [
        "0.79626095",
        "Arabic",
        "true",
        "6"
        ]
    }},
    {{
        "id": 1,
        "features": [
        "0.72317415",
        "French",
        "true",
        "9"
        ]
    }},
    {{
        "id": 2,
        "features": [
        "0.2532931",
        "German",
        "true",
        "7"
        ]
    }},
    {{
        "id": 3,
        "features": [
        "0.80586946",
        "German",
        "true",
        "6"
        ]
    }},
    {{
        "id": 4,
        "features": [
        "0.7160485",
        "French",
        "false",
        "9"
        ]
    }}
    ]
}}
}}"""


def main(api_token, cluster_id):
    
    # 1. Create model via API
    api_util = apiUtils.ApiUtil(api_token)
    model_id = api_util.create_line_item_model(model_schema.format(id=cluster_id))

    try:
        # 2. Connect a ubiquity-client
        Ubiquity.setCredential(api_token)
        if Ubiquity.openClient(cluster_id) != Ubiquity.SUCCESS:
            raise RuntimeError("Error during connection")
        client_handle = Ubiquity.getClientHandle()

        # 3. Prepare data and feedback streams
        feedback_path = "./datasets/feedback.csv"
        bid_request_path = "./datasets/features.csv"

        u.unzip(f"{bid_request_path}.gz", bid_request_path)
        u.unzip(f"{feedback_path}.gz", feedback_path)


        next_send_time = time.time_ns()

        max_cpm = 2.0
        creative_ids = [1, 2, 3, 4, 5]

        bid_requests = d.load_bid_requests(bid_request_path)
        feedbacks = d.load_feedbacks(feedback_path)
        feedback_hash_map = {}

        num_queries = len(bid_requests)
        next_send_time = time.time_ns()

        # 4. Loop and send prediction queries with their associated feedbacks
        dump_period = 1000
        count = 0
        while count < num_queries:
            if time.time_ns() < next_send_time:
                continue
            next_send_time += PACE_IN_NANOS

            # 4.1 Prepare a line item query
            query_id = random.randint(1, 1e9)
            request = bid_requests[count]
            feedback = feedbacks[count]

            feedback_hash_map[query_id] = feedback
            client_handle.startLineItem(model_id, query_id, max_cpm)

            # Fill in a random subset of eligible creatives
            random.shuffle(creative_ids)
            num_eligible_creatives = max(1, random.randint(1, len(creative_ids)))
            for i in range(num_eligible_creatives):
                client_handle.putCreative(creative_ids[i])

            # Add features
            client_handle.putNominal(request.app_bundle)
            client_handle.putCategorical(request.imp_banner_pos)
            client_handle.putCategorical(request.local_time)
            client_handle.putNominal(request.device_os)
            client_handle.putNominal(request.device_geo_city)
            client_handle.putNominal(request.device_model)
            client_handle.putNominal(request.bcat)
            client_handle.putNominal(request.device_geo_zip)
            client_handle.putNominal(request.user_yob)
            client_handle.putNominal(request.app_publisher_name)
            client_handle.putCategorical(request.device_connection_type)
            client_handle.putNominal(request.device_geo_region)
            client_handle.putNominal(request.app_cat)
            client_handle.putNominal(request.device_make)
            client_handle.putNominal(request.device_hxw)
            client_handle.putCategorical(request.user_gender)
            client_handle.putNominal(request.device_language)
            client_handle.putNominal(request.imp_display_manager)
            client_handle.putNominal(request.imp_banner_battr)
            client_handle.putNominal(request.device_geo_country)
            client_handle.putNominal(request.app_content_language)
            client_handle.putCategorical(request.app_content_context)
            client_handle.putBoolean(request.imp_banner_top_frame)
            client_handle.putCategorical(request.device_type)
            client_handle.putBoolean(request.imp_instl)
            client_handle.putNominal(request.app_page_cat)

            # 4.2 Send the prediction request
            if client_handle.predict():
                count += 1

            # 4.3 Poll prediction response and validate its QueryId", flush=True
            deadline = time.time_ns() + 10  # POLL_TIMEOUT
            while time.time_ns() < deadline:
                if client_handle.poll() == "SUCCESS":
                    break

            conversion_probability = client_handle.getProbability()
            creative_id = client_handle.getCreativeId()
            price = client_handle.getPrice()
            query_id = client_handle.getQueryId()
            feedback = feedback_hash_map[query_id]

            # 4.4 Sending back conversion feedback (if any)", flush=True
            client_handle.train(model_id, query_id, 0)  # Notify execution
            if feedback.level1:
                client_handle.train(model_id, query_id, 1)
            if feedback.level2:
                client_handle.train(model_id, query_id, 2)

            if count % dump_period == 0:
                print(f"Total queries = {count}, last response: predicted price = {price}, "
                        f"conversion probability = {conversion_probability}, selected creative id = {creative_id}", flush=True)

    finally:
        # 5. Wait for feedbacks to be processed and delete the models
        time.sleep(10)
        api_util.delete_model(model_id)
        client_handle.Close()
        Ubiquity.closeClient()

if __name__ == "__main__":
    import sys
    if len(sys.argv) < 3:
        raise RuntimeError("You need to provide API token and cluster ID to run the LineItemExample")
    main(sys.argv[1], sys.argv[2])


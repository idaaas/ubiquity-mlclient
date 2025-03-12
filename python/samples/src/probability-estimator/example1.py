import time
import random
import itertools
from datetime import datetime, timedelta
import json

import requests
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
    "name": "probabilityEstimator/Example1",
    "feedback_delays": [
    65,
    130
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
    ]
}}"""


def main(api_token, cluster_id, num_queries):
    
    # 1. Create model via API
    api_util = apiUtils.ApiUtil(api_token)
    model_id = api_util.create_probability_estimator(model_schema.format(id=cluster_id))

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

        bid_requests = d.load_bid_requests(bid_request_path)
        feedbacks = d.load_feedbacks(feedback_path)
        feedback_hash_map = {}

        next_send_time = time.time_ns()

        # 4. Loop and send prediction queries with their associated feedbacks
        dump_period = 500
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
            client_handle.startProbability(model_id, query_id)
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
            deadline = time.time_ns() + 200000000 # POLL_TIMEOUT
            status_code = Ubiquity.NO_DATA
            while time.time_ns() < deadline:
                status_code = client_handle.poll()
                if status_code != Ubiquity.NO_DATA:
                    break
            if status_code != Ubiquity.SUCCESS:
                continue

            # 4.4 read the response data
            conversion_probability = client_handle.getProbability()

            # 4.5 Sending back feedback (when applicable)

            query_id = client_handle.getQueryId()
            feedback = feedback_hash_map[query_id]

            # 4.5.1 send a positive feedback of level 0 to the cluster to execute the query.
		        # After this call, you cannot cancel the query
            client_handle.train(model_id, query_id, 0)  # Notify execution
            # 4.5.2 send a positive feedback of level 1 only when it is positive
            if feedback.level1:
                client_handle.train(model_id, query_id, 1)

            if count % dump_period == 0:
                print(f"Total queries = {count}, last response: conversion probability = {conversion_probability:.7f}", flush=True)

    finally:
        # 5. Wait for feedbacks to be processed and delete the models
        time.sleep(10)
        api_util.delete_model(model_id)
        client_handle.close()
        Ubiquity.closeClient()

if __name__ == "__main__":
    import sys
    if len(sys.argv) < 3:
        raise RuntimeError("You need to provide API token and cluster ID to run the LineItemExample")
    numQueries = 500000 if len(sys.argv) < 4 else int(sys.argv[3])
    main(sys.argv[1], sys.argv[2], numQueries)


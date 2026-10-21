import time
import random
import sys

from Ubiquity import Ubiquity
from src.utils import apiUtils
from src.utils import unzip as u
from src.utils import data as d

# Constants
POLL_TIMEOUT_NANOS = 200000000  # nanoseconds
THROUGHPUT_IN_QPS = 300
PACE_IN_NANOS = int(1e9 / THROUGHPUT_IN_QPS)

model_schema = """
{{
    "cluster_id": "{id}",
    "name": "actionrate/Example1",
    "feedbacks": [
	{{
	   "name": "imp",
	   "max_delay": 65
	}},
	{{
		"name": "view",
		"max_delay": 130
	}},
	{{
		"name": "install",
		"max_delay": 196
	}}
	],
    "bid_ratio": 0.1,
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
        "name": "a1",
        "type": "categorical"
        }} 
    ],
    "values": [
        {{
        "id": 1,
        "features": [
            "0"
        ]
        }}
    ]
}}
}}"""

def cleanup(client_handle, client_opened, api_util, model_id):
    """Single cleanup routine used on every exit path.
    Order of cleanup : client, wait for the last feedbacks, delete the model.
    """
    # 1. Close the client
    try:
        if client_handle is not None:
            client_handle.close()
        if client_opened:
            Ubiquity.closeClient()
    except Exception as e:
        print(f"Couldn't close client : {e}", file=sys.stderr)

    # 2. Wait for feedbacks to be processed and delete the model (only if it was created)
    if api_util is not None and model_id:
        try:
            print(f"Waiting 10 seconds for the last feedback to be processed...")
            time.sleep(10)
            api_util.delete_model(model_id)
        except Exception as e:
            print(f"Couldn't delete created model : {e}", file=sys.stderr)


def main(api_token, cluster_id, num_queries):
    api_util = None
    model_id = None
    client_opened = False
    client_handle = None

    try:
        # 1. Create model via API
        api_util = apiUtils.ApiUtil(api_token)
        model_id = api_util.create_model("action-rate", model_schema.format(id=cluster_id))
        if not model_id:
            raise RuntimeError("Failed to create model")

        # 2. Connect a ubiquity-client
        Ubiquity.setCredentials(api_token)
        if Ubiquity.openClient(cluster_id) != Ubiquity.SUCCESS:
            raise RuntimeError("Error during connection")
        client_opened = True

        client_handle = Ubiquity.getClientHandle()

        # 3. Prepare data and feedback streams
        feedback_path = "./datasets/feedback.csv"
        bid_request_path = "./datasets/features.csv"

        u.unzip(f"{bid_request_path}.gz", bid_request_path)
        u.unzip(f"{feedback_path}.gz", feedback_path)

        bid_price = 2.0
        adversary_price = bid_price / 2.0
        creative_id = 1

        bid_requests = d.load_requests(bid_request_path)
        feedbacks = d.load_feedbacks(feedback_path)
        feedback_hash_map = {}

        # 4. Loop and send prediction queries with their associated feedbacks
        next_send_time = time.monotonic_ns()
        dump_period = 500
        count = 0
        while count < num_queries:
            if time.monotonic_ns() < next_send_time:
                continue
            next_send_time += PACE_IN_NANOS

            # 4.1 Prepare an action rate query
            query_id = random.randint(1, 1e9)
            request = bid_requests[count]
            feedback = feedbacks[count]
            feedback_hash_map[query_id] = feedback

            client_handle.startActionRate(bid_price)

            # fill in the creatives (only one eligible in this example)
            client_handle.putCreative(creative_id)
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
            if client_handle.predict(model_id, query_id):
                count += 1

            # 4.3 Poll prediction response and validate its QueryId", flush=True
            deadline = time.monotonic_ns() + POLL_TIMEOUT_NANOS
            status_code = Ubiquity.NO_DATA
            while time.monotonic_ns() < deadline:
                status_code = client_handle.poll()
                if status_code != Ubiquity.NO_DATA:
                    break
            if status_code != Ubiquity.SUCCESS:
                continue

            # 4.4 read the response data
            conversion_probability = client_handle.getProbability()
            creative_id = client_handle.getCreativeId() # in this example, there is only one creative, so it will always be selected
            price = client_handle.getPrice() # the proposed bid price

            # 4.5 Sending back feedback (if when applicable)
            # no feedback should be sent to the cluster if the bid did not win the auction.
            # here we suppose that there is only one adversary in the auction, it bids flat with a predefined price.
            # if the bid price is larger than the adversary price, then bid price has won the auction, and feedback should be sent to the cluster.
            if price > adversary_price:
                query_id = client_handle.getQueryId()
                feedback = feedback_hash_map[query_id]

                # 4.5.1 send a positive feedback of level 0 to the cluster notifyong it that the auction has been won, incurring an impression.
                client_handle.train(model_id, query_id, "imp")  # Notify execution
                # 4.5.2 send a positive feedback of level 1 only when it is positive
                if feedback.level1:
                    client_handle.train(model_id, query_id, "view")
                # 4.5.3 send a positive feedback of level 2 only when it is positive
                if feedback.level2:
                    client_handle.train(model_id, query_id, "install")

            if count % dump_period == 0:
                print(f"Total queries = {count}, last response: predicted price = {price}, conversion probability = {conversion_probability:.7f}, selected creative id = {creative_id}", flush=True)

    finally:
         # 5. Single cleanup path on every exit (success, failure)
         cleanup(client_handle, client_opened, api_util, model_id)

if __name__ == "__main__":
    if len(sys.argv) < 3:
        raise RuntimeError("You need to provide API token and cluster ID to run the ActionRateExample")
    numQueries = 500000 if len(sys.argv) < 4 else int(sys.argv[3])
    main(sys.argv[1], sys.argv[2], numQueries)


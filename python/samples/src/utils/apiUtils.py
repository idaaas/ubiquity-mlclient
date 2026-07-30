import requests
import time
import json

class ApiUtil:
    def __init__(self, api_token):
        self.api_token = api_token
        self.base_url = "https://api.ubiquity.ai/v1"

    def create_probability_estimator(self, probability_estimator_body):
        print("Creating a probability-estimator model...")
        endpoint = f"{self.base_url}/models/probability-estimator"
        return self._create_model(endpoint, probability_estimator_body)

    def create_line_item_model(self, line_item_body):
        print("Creating a line-item model...")
        endpoint = f"{self.base_url}/models/line-item"
        return self._create_model(endpoint, line_item_body)
    
    def create_action_rate_model(self, action_rate_body):
        print("Creating a action-rate model...")
        endpoint = f"{self.base_url}/models/action-rate"
        return self._create_model(endpoint, action_rate_body)

    def create_binary_classification(self, binary_classification_body):
        print("Creating a binary-classification model...")
        endpoint = f"{self.base_url}/models/binary-classification"
        return self._create_model(endpoint, binary_classification_body)

    def _create_model(self, endpoint, model_body):
        headers = {
            "accept": "application/json",
            "content-type": "application/json",
            "authorization": f"Bearer {self.api_token}"
        }
        response = requests.post(endpoint, headers=headers, data=model_body)
        if response.status_code != 202:
            raise RuntimeError(f"Create model request returned {response.status_code} error code")
        
        resp_body = response.json()
        operation_id = resp_body.get("operation_id")

        operation = self._wait_operation(operation_id)
        model_id = operation.get("resource_id")
        print(f"Model created successfully, generated modelId={model_id}")
        return model_id

    def delete_model(self, model_id):
        print(f"Deleting a model with id={model_id}...")
        endpoint = f"{self.base_url}/models/{model_id}"
        headers = {
            "accept": "application/json",
            "authorization": f"Bearer {self.api_token}"
        }
        response = requests.delete(endpoint, headers=headers)
        if response.status_code != 202:
            raise RuntimeError(f"Delete model request returned {response.status_code} error code")

        resp_body = response.json()
        operation_id = resp_body.get("operation_id")

        operation = self._wait_operation(operation_id)
        if not operation:
            raise RuntimeError("Failed to delete model")
        print("Model deleted successfully.")

    def _wait_operation(self, operation_id):
        while True:
            operation = self._get_operation(operation_id)
            status = operation.get("status")
            if status in ["succeeded", "failed"]:
                break
            print(f"Operation status: {status}. Waiting...")
            time.sleep(1)
        if status == "succeeded":
            return operation
        else:
            raise RuntimeError(f"Operation failed with status: {status}")

    def _get_operation(self, operation_id):
        endpoint = f"{self.base_url}/operations/{operation_id}"
        headers = {
            "accept": "application/json",
            "authorization": f"Bearer {self.api_token}"
        }
        response = requests.get(endpoint, headers=headers)
        if response.status_code != 200:
            raise RuntimeError(f"Get operation request returned {response.status_code} error code")
        return response.json()
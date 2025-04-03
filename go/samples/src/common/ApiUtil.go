package common

import (
	"encoding/json"
	"fmt"
	"io"
	"net/http"
	"strconv"
	"strings"
	"time"
)

func CreateProbabilityEstimatorModel(apiToken string, probaEstimatorBody string) string {
	fmt.Println("Creating a probability-estimator model...")
	probaEstimatorEndpoint := "https://api.ubiquity.ai/v1/models/probability-estimator"
	return createModel(apiToken, probaEstimatorEndpoint, probaEstimatorBody)
}

func CreateLineItemModel(apiToken string, lineItemBody string) string {
	fmt.Println("Creating a line-item model...")
	lineItemEndpoint := "https://api.ubiquity.ai/v1/models/line-item"
	return createModel(apiToken, lineItemEndpoint, lineItemBody)
}

func CreateActionRateModel(apiToken string, actionRateBody string) string {
	fmt.Println("Creating a action-rate model...")
	actionRateEndpoint := "https://api.ubiquity.ai/v1/models/action-rate"
	return createModel(apiToken, actionRateEndpoint, actionRateBody)
}

type ModelResponseBody struct {
	Operation_id string
}

type Operation struct {
	Status      string
	Resource_id string
}

func createModel(apiToken string, endpoint string, modelBody string) string {
	req, err := http.NewRequest("POST", endpoint, strings.NewReader(modelBody))
	if err != nil {
		fmt.Println(err)
		return ""
	}

	req.Header.Add("Accept", "application/json")
	req.Header.Add("Content-Type", "application/json")
	req.Header.Add("Authorization", "Bearer "+apiToken)

	responseMessage, err := http.DefaultClient.Do(req)
	if err != nil {
		fmt.Println(err)
		return ""
	}
	if responseMessage.StatusCode != 202 {
		fmt.Println("Create model request returned " + strconv.Itoa(responseMessage.StatusCode) + " error code")
		return ""
	}
	body, err := io.ReadAll(responseMessage.Body)
	if err != nil {
		fmt.Println(err)
		return ""
	}

	var responseBody ModelResponseBody
	err = json.Unmarshal(body, &responseBody)
	if err != nil {
		fmt.Println(err)
		return ""
	}

	operation := getOperation(apiToken, responseBody.Operation_id)
	if operation == nil {
		return ""
	}
	for operation.Status == "pending" || operation.Status == "running" {
		operation = getOperation(apiToken, responseBody.Operation_id)
		time.Sleep(time.Second)
	}
	if operation.Status != "succeeded" {
		fmt.Println("Expected operation to be `succeeded`, got `" + operation.Status + "` instead")
		return ""
	}

	fmt.Println("Model created successfully, generated model ID=", operation.Resource_id)
	return operation.Resource_id
}

func DeleteModel(apiToken string, modelId string) string {
	fmt.Println("Deleting a model with id=", modelId)
	req, err := http.NewRequest("DELETE", "https://api.ubiquity.ai/v1/models/"+modelId, nil)
	if err != nil {
		fmt.Println(err)
		return ""
	}

	req.Header.Add("Accept", "application/json")
	req.Header.Add("Authorization", "Bearer "+apiToken)

	responseMessage, err := http.DefaultClient.Do(req)
	if err != nil {
		fmt.Println(err)
		return ""
	}
	if responseMessage.StatusCode != 202 {
		fmt.Println("Delete model request returned " + strconv.Itoa(responseMessage.StatusCode) + " error code")
		return ""
	}
	body, err := io.ReadAll(responseMessage.Body)
	if err != nil {
		fmt.Println(err)
		return ""
	}

	var responseBody ModelResponseBody
	err = json.Unmarshal(body, &responseBody)
	if err != nil {
		fmt.Println(err)
		return ""
	}

	operation := getOperation(apiToken, responseBody.Operation_id)
	if operation == nil {
		return ""
	}
	for operation.Status == "pending" || operation.Status == "running" {
		operation = getOperation(apiToken, responseBody.Operation_id)
		time.Sleep(time.Second)
	}
	if operation.Status != "succeeded" {
		fmt.Println("Expected operation to be `succeeded`, got `" + operation.Status + "` instead")
		return ""
	}
	fmt.Println("Model deleted successfully.")
	return operation.Resource_id
}

func getOperation(apiToken string, operationId string) *Operation {
	req, err := http.NewRequest("GET", "https://api.ubiquity.ai/v1/operations/"+operationId, nil)
	if err != nil {
		fmt.Println("Failed to create request to retrieve operation. Error : ", err)
		return nil
	}

	req.Header.Add("Accept", "application/json")
	req.Header.Add("Content-Type", "application/json")
	req.Header.Add("Authorization", "Bearer "+apiToken)

	responseMessage, err := http.DefaultClient.Do(req)
	if err != nil {
		fmt.Println("Failed to send request to retrieve operation. Error : ", err)
		return nil
	}
	body, err := io.ReadAll(responseMessage.Body)
	if err != nil {
		fmt.Println("Failed to read operation response. Error : ", err)
		return nil
	}

	operation := new(Operation)
	err = json.Unmarshal(body, operation)
	if err != nil {
		fmt.Println("Failed to unmarshal operation response. Error : ", err)
		return nil
	}
	return operation
}

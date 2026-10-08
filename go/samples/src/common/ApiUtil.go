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

const baseURL = "https://api.ubiquity.ai/v1"

type ModelResponseBody struct {
	Operation_id string
}

type Operation struct {
	Status      string
	Resource_id string
}

func CreateModel(modelType string, modelBody string, apiToken string) string {
	fmt.Println("Creating a", modelType, "model..............")
	
	endpoint := fmt.Sprintf("%s/models/%s", baseURL, modelType)
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

	operation := GetOperation(apiToken, responseBody.Operation_id)
	if operation == nil {
		return ""
	}
	for operation.Status == "pending" || operation.Status == "running" {
		operation = GetOperation(apiToken, responseBody.Operation_id)
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
	
	endpoint := fmt.Sprintf("%s/models/%d", baseURL, modelId)
	req, err := http.NewRequest("DELETE", endpoint, nil)
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

	operation := GetOperation(apiToken, responseBody.Operation_id)
	if operation == nil {
		return ""
	}
	for operation.Status == "pending" || operation.Status == "running" {
		operation = GetOperation(apiToken, responseBody.Operation_id)
		time.Sleep(time.Second)
	}
	if operation.Status != "succeeded" {
		fmt.Println("Expected operation to be `succeeded`, got `" + operation.Status + "` instead")
		return ""
	}
	fmt.Println("Model deleted successfully.")
	return operation.Resource_id
}

func GetOperation(apiToken string, operationId string) *Operation {
	
	endpoint := fmt.Sprintf("%s/operations/%d", baseURL, operationId)
	req, err := http.NewRequest("GET", endpoint, nil)
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

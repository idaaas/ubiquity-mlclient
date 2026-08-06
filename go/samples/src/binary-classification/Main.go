package main

import (
	"fmt"
	"math/rand"
	"os"
	"runtime"
	"samples/Ubiquity"
	"samples/src/common"
	"strconv"
	"time"
)

const POLL_TIMEOUT time.Duration = time.Millisecond * 200

func main() {
	os.Exit(example1())
}

func example1() int {
	if len(os.Args) < 3 {
		fmt.Println("You need to provide API token and cluster ID to run the binary classification example")
		return 1
	}
	apiToken := os.Args[1]
	clusterId := os.Args[2]
	numQueries := 500000
	var e error
	if len(os.Args) > 3 {
		numQueries, e = strconv.Atoi(os.Args[3])
		if e != nil {
			panic(fmt.Sprintf("%s must be an integer", os.Args[3]))
		}
	}

	// 1. Create your models via API
	var schema string = getBinaryClassificationBody(clusterId)
	var modelId string = common.CreateBinaryClassificationModel(apiToken, schema)
	if modelId == "" {
		return 1
	}
	defer common.DeleteModel(apiToken, modelId)

	// 2. Create and connect a ubiquity-client
	Ubiquity.SetCredentials(apiToken)
	statusCode := Ubiquity.OpenClient(clusterId)
	if Ubiquity.SUCCESS != statusCode {
		fmt.Println("Error during connection : ", Ubiquity.GetStatusString(statusCode))
		return 1
	}
	defer Ubiquity.CloseClient()

	clientHandle := Ubiquity.GetClientHandle()
	defer clientHandle.Close()

	// 3. Prepare data 
	// unzip datasets
	const requestPath string = "./datasets/features.csv"
	common.Unzip(requestPath+".gz", requestPath)
	// load Requests from file
	requests := common.LoadBidRequests(requestPath)
	// The next lines of code are useful for pacing the traffic sent to the ml cluster based on a fixed throughput
	pace := time.Second / 300
	nextSendNanoTime := time.Now()

	// 4. Loop and send prediction queries with their level_0 feedbacks
	const dumpPeriod int = 1000
	var count int = 0
	for count < numQueries {
		if time.Now().Before(nextSendNanoTime) {
			continue
		}
		nextSendNanoTime = nextSendNanoTime.Add(pace)

		// 4.1 prepare a binary classification query
		queryId := rand.Int63()
		// read a request from the dataset file
		request := requests[count]
		// prepare a binary classification query using this request
		clientHandle.StartBinaryClassification()
		// fill in the features as specified in the model schema in config.json
		clientHandle.PutNominal(request.AppBundle)
		clientHandle.PutCategorical(request.ImpBannerPos)
		clientHandle.PutCategorical(request.LocalTime)
		clientHandle.PutNominal(request.DeviceOs)
		clientHandle.PutNominal(request.DeviceGeoCity)
		clientHandle.PutNominal(request.DeviceModel)
		clientHandle.PutNominal(request.Bcat)
		clientHandle.PutNominal(request.DeviceGeoZip)
		clientHandle.PutNominal(request.UserYob)
		clientHandle.PutNominal(request.AppPublisherName)
		clientHandle.PutCategorical(request.DeviceConnectionType)
		clientHandle.PutNominal(request.DeviceGeoRegion)
		clientHandle.PutNominal(request.AppCat)
		clientHandle.PutNominal(request.DeviceMake)
		clientHandle.PutNominal(request.DeviceHxW)
		clientHandle.PutCategorical(request.UserGender)
		clientHandle.PutNominal(request.DeviceLanguage)
		clientHandle.PutNominal(request.ImpDisplayManager)
		clientHandle.PutNominal(request.ImpBannerBattr)
		clientHandle.PutNominal(request.DeviceGeoCountry)
		clientHandle.PutNominal(request.AppContentLanguage)
		clientHandle.PutCategorical(request.AppContentContext)
		clientHandle.PutBoolean(request.ImpBannerTopFrame)
		clientHandle.PutCategorical(request.DeviceType)
		clientHandle.PutBoolean(request.ImpInstl)
		clientHandle.PutNominal(request.AppPageCat)

		// 4.2. send the prediction request
		if clientHandle.Predict(modelId, queryId) == Ubiquity.SUCCESS {
			count++
		}

		// 4.3. wait to poll prediction response and validate its QueryId
		deadline := time.Now().Add(POLL_TIMEOUT)
		statusCode = clientHandle.Poll()
		for statusCode == Ubiquity.NO_DATA && time.Now().After(deadline) {
			runtime.Gosched()
			statusCode = clientHandle.Poll()
		}
		if statusCode != Ubiquity.SUCCESS {
			continue
		}

		// 4.4 read the response data
		predictionResponse := clientHandle.GetClassification()

		// 4.5 Sending back a positive feedback of level 0 to the cluster to execute the query.
		// After calling the feedback level 0, no cancellation is allowed
		clientHandle.Train(modelId, queryId, 0)

		if count%dumpPeriod == 0 {
			fmt.Printf("Total queries = %d, last response: classification response = %d\n", count, predictionResponse)
		}
	}

	// 5. wait for the last feedbacks sent to be processed by the cluster then delete the models
	time.Sleep(10 * time.Second)
	return 0
}

func getBinaryClassificationBody(clusterId string) string {
	return `
            {
              "cluster_id": "` + clusterId + `",
              "name": "code-example binary classification",
              "feedbacks": [
                {
					"name": "level_0",
					"max_delay": 65
				}
              ],
              "features": [
                {
                  "name": "app.bundle",
                  "type": "nominal"
                },
                {
                  "name": "imp.banner.pos",
                  "type": "categorical"
                },
                {
                  "name": "localtime",
                  "type": "categorical"
                },
                {
                  "name": "device.os",
                  "type": "nominal"
                },
                {
                  "name": "device.geo.city",
                  "type": "nominal"
                },
                {
                  "name": "device.model",
                  "type": "nominal"
                },
                {
                  "name": "bcat",
                  "type": "nominal"
                },
                {
                  "name": "device.geo.zip",
                  "type": "nominal"
                },
                {
                  "name": "user.yob",
                  "type": "nominal"
                },
                {
                  "name": "app.publisher.name",
                  "type": "nominal"
                },
                {
                  "name": "device.connectiontype",
                  "type": "categorical"
                },
                {
                  "name": "device.geo.region",
                  "type": "nominal"
                },
                {
                  "name": "app.cat",
                  "type": "nominal"
                },
                {
                  "name": "device.make",
                  "type": "nominal"
                },
                {
                  "name": "device.HxW",
                  "type": "nominal"
                },
                {
                  "name": "user.gender",
                  "type": "categorical"
                },
                {
                  "name": "device.language",
                  "type": "nominal"
                },
                {
                  "name": "imp.displaymanager",
                  "type": "nominal"
                },
                {
                  "name": "imp.banner.battr",
                  "type": "nominal"
                },
                {
                  "name": "device.geo.country",
                  "type": "nominal"
                },
                {
                  "name": "app.content.language",
                  "type": "nominal"
                },
                {
                  "name": "app.content.context",
                  "type": "categorical"
                },
                {
                  "name": "imp.banner.topframe",
                  "type": "boolean"
                },
                {
                  "name": "device.devicetype",
                  "type": "categorical"
                },
                {
                  "name": "imp.instl",
                  "type": "boolean"
                },
                {
                  "name": "app.pagecat",
                  "type": "nominal"
                }
              ]
            }`
}

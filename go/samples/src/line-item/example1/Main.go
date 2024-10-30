package main

//#cgo LDFLAGS: ./lib/Ubiquity/ubiquityclient.so -Wl,-rpath=./lib/Ubiquity/
import "C"

import (
	"fmt"
	"math/rand"
	"os"
	"runtime"
	"samples/lib/Ubiquity"
	"samples/src/common"
	"time"
)

const POLL_TIMEOUT time.Duration = time.Millisecond * 200

func main() {
	os.Exit(example1())
}

func example1() int {
	if len(os.Args) < 3 {
		fmt.Println("You need to provide API token and cluster ID to run the LineItemExample")
		return 1
	}
	apiToken := os.Args[1]
	clusterId := os.Args[2]

	// 1. Create your models via API
	var lineItemSchema string = getLineItemBody(clusterId)
	var modelId string = common.CreateLineItemModel(apiToken, lineItemSchema)
	if modelId == "" {
		return 1
	}
	defer common.DeleteModel(apiToken, modelId)

	// 2. Create and connect a ubiquity-client
	Ubiquity.SetCredential(apiToken)
	statusCode := Ubiquity.OpenClient(clusterId)
	if Ubiquity.SUCCESS != statusCode {
		fmt.Println("Error during connection : ")
		return 1
	}
	defer Ubiquity.CloseClient()

	clientHandle := Ubiquity.GetClientHandle()
	defer clientHandle.Close()

	// 3. Prepare data and feedback streams
	const maxCPM float32 = 2.0
	creativeIds := [5]int64{1, 2, 3, 4, 5} // a list of all creatives ids used when creating the model
	// load BidRequest data and Feedback data from files
	const feedbackPath string = "./rsc/datasets/feedback.csv"
	const bidRequestPath string = "./rsc/datasets/features.csv"
	bidRequests := common.LoadBidRequests(bidRequestPath)
	feedbacks := common.LoadFeedbacks(feedbackPath)
	feedbackHashMap := make(map[int64]common.Feedback)
	numQueries := len(bidRequests)

	// The next lines of code are useful for pacing the traffic sent to the ml cluster based on a fixed throughput
	pace := time.Second / 300
	nextSendNanoTime := time.Now()

	// 4. Loop and send prediction queries with their associated feedbacks
	const dumpPeriod int = 1000
	var count int = 0
	for count < numQueries {
		if time.Now().Before(nextSendNanoTime) {
			continue
		}
		nextSendNanoTime = nextSendNanoTime.Add(pace)

		// 4.1 prepare a line item query
		queryId := rand.Int63()
		// read a bid request with its feedback from the dataset file
		request := bidRequests[count]
		feedback := feedbacks[count]
		// read the feedback for all the levels and store it in a temp map
		feedbackHashMap[queryId] = feedback
		// prepare a line item query using this bid request
		clientHandle.StartLineItem(modelId, queryId, maxCPM)
		// fill in a random subset of eligible creatives
		rand.Shuffle(len(creativeIds), func(i int, j int) { creativeIds[i], creativeIds[j] = creativeIds[j], creativeIds[i] })
		numEligibleCreatives := rand.Intn(len(creativeIds)) + 1 // at least one creative
		for i := 0; i < numEligibleCreatives; i++ {
			clientHandle.PutCreative(creativeIds[i])
		}
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
		if clientHandle.Predict() == Ubiquity.SUCCESS {
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
			continue;
		}

		conversionProbability := clientHandle.GetProbability()
		creativeId := clientHandle.GetCreativeId()
		price := clientHandle.GetPrice()
		queryId = clientHandle.GetQueryId()
		feedback = feedbackHashMap[queryId]
		// send positive feedback only if conversion occurred
		// 4.5 Sending back conversion feedback (if any)
		// 4.5.1. send a notification of execution
		// notifies the cluster that the predicted pair (price, creative) have entered the auction
		// here we suppose that all predictions actually enter the auction
		// if the cluster is not notified of the execution, no feedback will be considered for the given query
		clientHandle.Train(modelId, queryId, 0)
		// 4.5.2. Try to send feedback of level 1
		if feedback.Level1 {
			clientHandle.Train(modelId, queryId, 1)
		}
		// 4.5.3. Try to send feedback of level 2
		if feedback.Level2 {
			clientHandle.Train(modelId, queryId, 2)
		}

		if count%dumpPeriod == 0 {
			fmt.Printf("Total queries = %d, last response: predicted price = %f, conversion probability = %f, selected creative id = %d\n", count, price, conversionProbability, creativeId)
		}
	}

	// 5. wait for the last feedbacks sent to be processed by the cluster then delete the models
	time.Sleep(10 * time.Second)
	return 0
}

func getLineItemBody(clusterId string) string {
	return `
            {
              "cluster_id": "` + clusterId + `",
              "name": "code-example line-item",
              "feedback_delays": [
                20,
                30,
                40
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
              ],
            "creatives": {
              "features": [
                {
                  "name": "a0",
                  "type": "numeric"
                },
                {
                  "name": "a1",
                  "type": "nominal"
                },
                {
                  "name": "a2",
                  "type": "boolean"
                },
                {
                  "name": "a3",
                  "type": "categorical"
                }
              ],
              "values": [
                {
                  "id": 5,
                  "features": [
                    "0.79626095",
                    "Arabic",
                    "true",
                    "6"
                  ]
                },
                {
                  "id": 1,
                  "features": [
                    "0.72317415",
                    "French",
                    "true",
                    "9"
                  ]
                },
                {
                  "id": 2,
                  "features": [
                    "0.2532931",
                    "German",
                    "true",
                    "7"
                  ]
                },
                {
                  "id": 3,
                  "features": [
                    "0.80586946",
                    "German",
                    "true",
                    "6"
                  ]
                },
                {
                  "id": 4,
                  "features": [
                    "0.7160485",
                    "French",
                    "false",
                    "9"
                  ]
                }
              ]
            }
            }`
}

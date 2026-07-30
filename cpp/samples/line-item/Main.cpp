#include "../common/ApiUtil.hpp"
#include "../common/FileUtil.hpp"
#include "../../include/ubiquity_client/Ubiquity.hpp"

#include <algorithm>
#include <chrono>
#include <unordered_map>
#include <random>
#include <iostream>
#include <cstdlib>
#include <signal.h>

// For how long should the `poll` method be called before considering there's nothing to poll for ?
constexpr const int64_t POLL_TIMEOUT_MS = 200;
Ubiquity::ClientHandle clientHandle;
const char* modelId;
const char* apiToken;

void sigHandler(int s) {
    std::cout << "Caught signal " << s << std::endl;
    // 5. close the handle and the whole client
    clientHandle.close();
    Ubiquity::closeClient();
    // 6. wait for the last feedback sent to be processed by the cluster then delete the models
    std::cout << "Waiting 10 seconds for the last feedback sent to be processed by the cluster.........." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(10));
    Common::deleteModel(modelId, apiToken);
    exit(1);
}

// List of types corresponding to the structure of a bid request and used by the CSV parser to fill that kind of structure.
std::vector<std::string> bidRequestStructure = {
    "appBundle", "nominal",
    "impBannerPos", "categorical",
    "localTime", "categorical",
    "deviceOs", "nominal",
    "deviceGeoCity", "nominal",
    "deviceModel", "nominal",
    "bcat", "nominal",
    "deviceGeoZip", "nominal",
    "userYob", "nominal",
    "appPublisherName", "nominal",
    "deviceConnectionType", "categorical",
    "deviceGeoRegion", "nominal",
    "appCat", "nominal",
    "deviceMake", "nominal",
    "deviceHxW", "nominal",
    "userGender", "categorical",
    "deviceLanguage", "nominal",
    "impDisplayManager", "nominal",
    "impBannerBattr", "nominal",
    "deviceGeoCountry", "nominal",
    "appContentLanguage", "nominal",
    "appContentContext", "categorical",
    "impBannerTopFrame", "boolean",
    "deviceType", "categorical",
    "impInstl", "boolean",
    "appPageCat", "nominal",
};

// List of types corresponding to the structure of feedback and used by the CSV parser to fill that kind of structure.
std::vector<std::string> feedbackStructure = {
    "level0", "boolean",
    "level1", "boolean",
    "level2", "boolean",
};

// Returns the JSON body to use during the API call to create the line-item model.
static const std::string getLineItem(const std::string &clusterId)
{
    std::string body = R"({"cluster_id":")" + clusterId + R"(",
          "name": "code-example line-item",
          "feedbacks": [
             {
                "name": "imp",
                "max_delay": 65
             },
             {
                "name": "view",
                "max_delay": 130
             },
             {
                "name": "install",
                "max_delay": 196
             }
            ],
    )";
    body += R"("features": [)";
    for (size_t index = 0; index < bidRequestStructure.size(); index += 2) {
        if (index != 0) {
            body += ",\n";
        }
        body += R"({"name": ")" + bidRequestStructure[index] + R"(","type": ")" + bidRequestStructure[index + 1] + "\"}";
    }
    body += R"(],
          "creatives": {
            "features": [
              {
                "name": "a1",
                "type": "categorical"
              }
            ],
            "values": [
              {
                "id": 1,
                "features": [
                    "0"
                ]
              }
            ]
          }
        }
    )";
    return body;
}

// Example program for line-item prediction.
int main(const int argc, const char **argv) {

    signal (SIGINT,sigHandler);

    if (argc < 2) {
        std::cout << "Error : You need to provide an API token and a cluster ID to run the line-item example." << std::endl;
        return 1;
    }
    apiToken = argv[1];
    const auto clusterId = argv[2];
    if (argc < 3) {
        std::cout << "You need to provide API token and cluster ID to run the LineItemExample" << std::endl;
        return 1;
    }
    const size_t numQueries = argc >= 4 ? std::stoi(argv[3]) : 500000;

    // 1. Create your models via API
    const auto modelIdString = Common::createModel("line-item", getLineItem(clusterId), apiToken);
    if (modelIdString == "") {
        return 1;
    }

    modelId = modelIdString.c_str();

    // 2. Create and connect a ubiquity-client
    Ubiquity::setCredentials(apiToken);
    auto statusCode = Ubiquity::openClient(clusterId);
    if (Ubiquity::SUCCESS != statusCode) {
        std::cout << "Error during connection : " << Ubiquity::getStatusString(statusCode) << std::endl;
        Common::deleteModel(modelId, apiToken);
        return 1;
    }

    clientHandle = Ubiquity::getClientHandle();

    // 3. Prepare data and feedback streams

    // Unzip datasets.
    const auto bidRequestPath = "./datasets/features.csv";
    const auto feedbackPath = "./datasets/feedback.csv";
    if(std::system((std::string("gzip -d ") + bidRequestPath + ".gz 2>&1").c_str())) {
      std::cout << "Error unzipping " << bidRequestPath << std::endl;
      return 1;
    }
    if(std::system((std::string("gzip -d ") + feedbackPath + ".gz 2>&1").c_str())) {
      std::cout << "Error unzipping " << feedbackPath << std::endl;
      return 1;
    }

    // Useful parameters for line item queries
    const float maxCPM = 2.0f;
    const float adversaryPrice = maxCPM / 2.0f;
    int creativeId = 1;

    // Random generator
    std::mt19937_64 randgen;
    randgen.seed(std::random_device{}());

    // Load bid request data and feedback data from CSV files.
    auto bidRequests = Common::loadDataFromCsvFile(bidRequestPath, bidRequestStructure);
    auto feedbacks = Common::loadDataFromCsvFile(feedbackPath, feedbackStructure);

    // This map will serve to retrieve a feedback to send for a corresponding query ID.
    std::unordered_map<int64_t, Common::CsvDataMap> feedbackHashMap{};

    // The next lines of code are useful for pacing the traffic sent to the ml cluster based on a fixed throughput
    const auto pace = std::chrono::milliseconds(20); // 50 query / second
    auto nextSendTime = std::chrono::high_resolution_clock::now();

    // How often to print progress in the console.
    const size_t dumpPeriod = 1000;

    // 4. Loop and send prediction queries with their associated feedbacks.
    size_t count = 0;
    while (count < numQueries) {
        if (std::chrono::high_resolution_clock::now() < nextSendTime) {
            continue;
        }
        nextSendTime = nextSendTime + pace;

        // 4.1 prepare a line item query
        int64_t queryId = randgen();
        // read a bid request from the dataset file
        auto &request = bidRequests[count];
        // read the feedback for all the levels from the dataset file and store it in a temp map
        feedbackHashMap[queryId] = feedbacks[count];
        // prepare a line item query using this bid request
        clientHandle.startLineItem(maxCPM);
        // fill in the creatives (only one eligible in this example)
        clientHandle.putCreative(creativeId);

        // fill in the features as specified in the model schema
        clientHandle.putNominal    (request["appBundle"].nominal.c_str());
        clientHandle.putCategorical(request["impBannerPos"].categorical);
        clientHandle.putCategorical(request["localTime"].categorical);
        clientHandle.putNominal    (request["deviceOs"].nominal.c_str());
        clientHandle.putNominal    (request["deviceGeoCity"].nominal.c_str());
        clientHandle.putNominal    (request["deviceModel"].nominal.c_str());
        clientHandle.putNominal    (request["bcat"].nominal.c_str());
        clientHandle.putNominal    (request["deviceGeoZip"].nominal.c_str());
        clientHandle.putNominal    (request["userYob"].nominal.c_str());
        clientHandle.putNominal    (request["appPublisherName"].nominal.c_str());
        clientHandle.putCategorical(request["deviceConnectionType"].categorical);
        clientHandle.putNominal    (request["deviceGeoRegion"].nominal.c_str());
        clientHandle.putNominal    (request["appCat"].nominal.c_str());
        clientHandle.putNominal    (request["deviceMake"].nominal.c_str());
        clientHandle.putNominal    (request["deviceHxW"].nominal.c_str());
        clientHandle.putCategorical(request["userGender"].categorical);
        clientHandle.putNominal    (request["deviceLanguage"].nominal.c_str());
        clientHandle.putNominal    (request["impDisplayManager"].nominal.c_str());
        clientHandle.putNominal    (request["impBannerBattr"].nominal.c_str());
        clientHandle.putNominal    (request["deviceGeoCountry"].nominal.c_str());
        clientHandle.putNominal    (request["appContentLanguage"].nominal.c_str());
        clientHandle.putCategorical(request["appContentContext"].categorical);
        clientHandle.putBoolean    (request["impBannerTopFrame"].boolean);
        clientHandle.putCategorical(request["deviceType"].categorical);
        clientHandle.putBoolean    (request["impInstl"].boolean);
        clientHandle.putNominal    (request["appPageCat"].nominal.c_str());

        // 4.2. send the prediction request
        if (clientHandle.predict(modelId, queryId) == Ubiquity::SUCCESS) {
            count++;
        }

        // 4.3. wait to poll prediction response and validate its QueryId
        const auto deadline = std::chrono::high_resolution_clock::now() + std::chrono::milliseconds(POLL_TIMEOUT_MS);
        statusCode = clientHandle.poll();
        while (statusCode == Ubiquity::NO_DATA && std::chrono::high_resolution_clock::now() < deadline) {
            std::this_thread::yield();
            statusCode = clientHandle.poll();
        }
        if (statusCode != Ubiquity::SUCCESS) {
            continue;
        }

        // 4.4 read the response data
        const auto conversionProbability = clientHandle.getProbability();
        const auto creativeId = clientHandle.getCreativeId();
        const auto price = clientHandle.getPrice();

        // 4.5 Sending back feedback (if when applicable)
        // no feedback should be sent to the cluster if the bid did not win the auction.
        // here we suppose that there is only one adversary in the auction, it bids flat with a predefined price.
        // if the bid price is larger than the adversary price, then bid price has won the auction, and feedback should be sent to the cluster.
        if (price > adversaryPrice) {

          queryId = clientHandle.getQueryId();
          auto feedback = feedbackHashMap[queryId];
          
          // 4.5.1 send a positive feedback of level 0 to the cluster notifyong it that the auction has been won, incurring an impression.
          clientHandle.train(modelId, queryId, 0);
          // 4.5.2. Try to send feedback of level 1
          if (feedback["level1"].boolean) {
              clientHandle.train(modelId, queryId, 1);
          }
          // 4.5.3. Try to send feedback of level 2
          if (feedback["level2"].boolean) {
              clientHandle.train(modelId, queryId, 2);
          }
        }

        if (count % dumpPeriod == 0) {
            std::cout << "Total queries = " << count << ", last response: predicted price = " << price << ", conversion probability = " << std::setprecision(7) << conversionProbability << ", selected creative id = " << creativeId << std::endl;
        }
    }

    // 5. close the handle and the whole client
    clientHandle.close();
    Ubiquity::closeClient();

    // 6. wait for the last feedback sent to be processed by the cluster then delete the models
    std::cout << "Waiting 10 seconds for the last feedback sent to be processed by the cluster.........." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(10));
    Common::deleteModel(modelId, apiToken);
    return 0;
}


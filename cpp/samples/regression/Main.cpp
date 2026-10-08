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
#include <atomic>

// For how long should the `poll` method be called before considering there's nothing to poll for ?
constexpr const int64_t POLL_TIMEOUT_MS = 200;
Ubiquity::ClientHandle clientHandle;
const char* modelId;
const char* apiToken;
std::atomic<bool> g_interrupted{false};
bool clientOpened = false;
bool modelCreated = false;

void sigHandler(int) {
    // set a flag
    g_interrupted.store(true, std::memory_order_relaxed);
}

// List of types corresponding to the structure of a request and used by the CSV parser to fill that kind of structure.
std::vector<std::string> requestStructure = {
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

// Returns the JSON body to use during the API call to create the regression model.
static const std::string buildModelSchema(const std::string &clusterId)
{
    std::string body = R"({"cluster_id":")" + clusterId + R"(",
          "name": "code-example regression",
          "feedbacks": [
            {
                "name": "level_0",
                "max_delay": 65
            }
          ],
    )";
    body += R"("features": [)";
    for (size_t index = 0; index < requestStructure.size(); index += 2) {
        if (index != 0) {
            body += ",\n";
        }
        body += R"({"name": ")" + requestStructure[index] + R"(","type": ")" + requestStructure[index + 1] + "\"}";
    }
    body += R"(]
        }
    )";
    return body;
}

// Example program for regression prediction.
int main(const int argc, const char **argv) {

    signal (SIGINT,sigHandler);

    if (argc < 2) {
        std::cout << "Error : You need to provide an API token and a cluster ID to run the regression example." << std::endl;
        return 1;
    }
    apiToken = argv[1];
    const auto clusterId = argv[2];
    if (argc < 3) {
        std::cout << "You need to provide API token and cluster ID to run the regression example" << std::endl;
        return 1;
    }
    const size_t numQueries = argc >= 4 ? std::stoi(argv[3]) : 500000;

    // Single cleanup routine used on every exit path.
    auto cleanup = [&]() {
        if (clientOpened) {
            clientHandle.close();
            Ubiquity::closeClient();
        }
        if (modelCreated) {
            std::cout << "Waiting 10 seconds for the last feedback to be processed..." << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(10));
            Common::deleteModel(modelId, apiToken);
        }
    };

    try {
        // 1. Create your models via API
        const auto modelIdString = Common::createModel("regression", buildModelSchema(clusterId), apiToken);
        if (modelIdString.empty()) {
            throw std::runtime_error("Failed to create model");
        }
        modelId = modelIdString.c_str();
        modelCreated = true;

        // 2. Create and connect a ubiquity-client
        Ubiquity::setCredentials(apiToken);
        auto statusCode = Ubiquity::openClient(clusterId);
        if (Ubiquity::SUCCESS != statusCode) {
            throw std::runtime_error(std::string("Error during connection: ") + Ubiquity::getStatusString(statusCode));
        }
        clientHandle = Ubiquity::getClientHandle();
        clientOpened = true;

        // 3. Prepare data
        const auto requestPath = "./datasets/features.csv";
        if(std::system((std::string("gzip -d ") + requestPath + ".gz 2>&1").c_str())) {
            throw std::runtime_error(std::string("Error unzipping ") + requestPath);
        }

        // Random generator
        std::mt19937_64 randgen;
        randgen.seed(std::random_device{}());

        // Load request data from CSV file.
        auto requests = Common::loadDataFromCsvFile(requestPath, requestStructure);

        // The next lines of code are useful for pacing the traffic sent to the ml cluster based on a fixed throughput
        const auto pace = std::chrono::microseconds(3333); // 300 query / second
        auto nextSendTime = std::chrono::high_resolution_clock::now();

        // How often to print progress in the console.
        const size_t dumpPeriod = 1000;

        // 4. Loop and send prediction queries with their level_0 feedbacks.
        size_t count = 0;
        while (count < numQueries && !g_interrupted.load(std::memory_order_relaxed)) {
            if (std::chrono::high_resolution_clock::now() < nextSendTime) {
                continue;
            }
            nextSendTime = nextSendTime + pace;

            // 4.1 prepare a regression query
            int64_t queryId = randgen();
            // read a request from the dataset file
            auto &request = requests[count];
            // prepare a regression query using this request
            clientHandle.startRegression();
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
            const auto result = clientHandle.getPrediction();

            // 4.5 Sending back a positive feedback of level_0 to the cluster to execute the query.
            // After calling the feedback level 0, no cancellation is allowed
            clientHandle.train(modelId, queryId, 0);

            if (count % dumpPeriod == 0) {
                std::cout << "Total queries = " << count << ", last response: result = " << result << std::endl;
            }
        }

        if (g_interrupted.load(std::memory_order_relaxed)) {
                std::cout << "Interrupted, shutting down..." << std::endl;
        }

        // 5. close the opened resources
        cleanup();
        return 0;
    }
    catch (const std::exception &e) {
        std::cout << "Fatal error: " << e.what() << std::endl;
        cleanup();
        return 1;
    }
}

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
// How often to print progress in the console.
constexpr size_t DUMP_PERIOD = 1000;
constexpr size_t DEFAULT_NUM_QUERIES = 500000;
// The throughput of prediction requests to ml cluster
constexpr int THROUGHPUT_IN_QPS = 300;

std::atomic<bool> g_interrupted{false};

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

// List of types corresponding to the structure of feedback and used by the CSV parser to fill that kind of structure.
std::vector<std::string> feedbackStructure = {
    "level0", "boolean",
    "level1", "boolean",
};

// Returns the JSON body to use during the API call to create the model.
static const std::string buildModelSchema(const std::string &clusterId)
{
    std::string body = R"({"cluster_id":")" + clusterId + R"(",
          "name": "code-example traffic-shaping",
          "feedbacks": [
            {
                "name": "bid",
                "max_delay": 65
            },
            {
                "name": "win",
                "max_delay": 130
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

int main(const int argc, const char **argv) {

    signal(SIGINT, sigHandler);
    signal(SIGTERM, sigHandler);

    if (argc < 3) {
        std::cout << "Error : You need to provide an API token and a cluster ID to run the example." << std::endl;
        return 1;
    }
    const char* apiToken = argv[1];
    const auto clusterId = argv[2];
    const size_t numQueries = argc >= 4 ? std::stoi(argv[3]) : DEFAULT_NUM_QUERIES;

    Ubiquity::ClientHandle clientHandle;
    std::string modelIdString; // outside the try to remain accessible in the catch 
    bool clientOpened = false;
    bool modelCreated = false;

    // Single cleanup routine used on every exit path.
    auto cleanup = [&]() {
        if (clientOpened) {
            clientHandle.close();
            Ubiquity::closeClient();
        }
        if (modelCreated) {
            std::cout << "Waiting 10 seconds for the last feedback to be processed..." << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(10));
            Common::deleteModel(modelIdString.c_str(), apiToken);
        }
    };

    try {
        // 1. Create your models via API
        modelIdString = Common::createModel("traffic-shaping", buildModelSchema(clusterId), apiToken);
        if (modelIdString.empty()) {
            throw std::runtime_error("Failed to create model");
        }
        const char* modelId = modelIdString.c_str();
        modelCreated = true;

        // 2. Create and connect a ubiquity-client
        Ubiquity::setCredentials(apiToken);
        auto statusCode = Ubiquity::openClient(clusterId);
        if (Ubiquity::SUCCESS != statusCode) {
            throw std::runtime_error(std::string("Error during connection: ") + Ubiquity::getStatusString(statusCode));
        }
        clientOpened = true;

        clientHandle = Ubiquity::getClientHandle();

        // 3. Prepare data and feedback streams
        const auto requestPath = "./datasets/features.csv";
        const auto feedbackPath = "./datasets/feedback.csv";
        if(std::system((std::string("gzip -d ") + requestPath + ".gz 2>&1").c_str())) {
            throw std::runtime_error(std::string("Error unzipping ") + requestPath);
        }
        if(std::system((std::string("gzip -d ") + feedbackPath + ".gz 2>&1").c_str())) {
            throw std::runtime_error(std::string("Error unzipping ") +  feedbackPath);
        }
        
        // Load request data and feedbacks from CSV file.
        auto requests = Common::loadDataFromCsvFile(requestPath, requestStructure);
        auto feedbacks = Common::loadDataFromCsvFile(feedbackPath, feedbackStructure);
        if (requests.empty() || feedbacks.empty()) {
            throw std::runtime_error("Empty dataset");
        }

        // This map will serve to retrieve a feedback to send for a corresponding query ID.
        std::unordered_map<int64_t, Common::CsvDataMap> feedbackHashMap{};

        // Pace the traffic sent to the ml cluster based on a fixed throughput
        const auto pace = std::chrono::nanoseconds(std::chrono::seconds(1)) / THROUGHPUT_IN_QPS;
        auto nextSendTime = std::chrono::high_resolution_clock::now();

        // Random generator
        std::mt19937_64 randgen;
        randgen.seed(std::random_device{}());

        // 4. Loop and send prediction queries with their associated feedbacks.
        size_t count = 0;
        while (count < numQueries && !g_interrupted.load(std::memory_order_relaxed)) {
            if (std::chrono::high_resolution_clock::now() < nextSendTime) {
                continue;
            }
            nextSendTime = nextSendTime + pace;

            // 4.1 prepare a traffic shaping query
            int64_t queryId = randgen();
            // read a request from the dataset file
            auto &request = requests[count];
            // read the feedback for all the levels from the dataset file and store it in a temp map
            feedbackHashMap[queryId] = feedbacks[count];
            // prepare the query using this request
            clientHandle.startTrafficShaping();
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
            const auto action = clientHandle.getAction();

            // 4.5 Sending back feedback (if when applicable)
            queryId = clientHandle.getQueryId();
            auto feedback = feedbackHashMap[queryId];

            // 4.5.1 send a positive feedback of level 0 to the cluster indicating that the DSP participated in the auction.
            clientHandle.train(modelId, queryId, "bid");
            // 4.5.2. Try to send feedback of level 1 to the cluster indicating that the DSP won the auction.
            if (feedback["level1"].boolean) {
                clientHandle.train(modelId, queryId, "win");
            }

            if (count % DUMP_PERIOD == 0) {
                std::cout << "Total queries = " << count << ", last response: action = " << (action == 0 ? "NO_SEND" : "SEND") << std::endl;
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

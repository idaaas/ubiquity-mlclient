#include <curl/curl.h>
#include <nlohmann/json.hpp>

#include <iostream>
#include <thread>

namespace Common {

static constexpr size_t CURL_OPERATION_TIME_LIMIT_MS = 30000L;


static size_t writeCurlRequestResponse(void *buffer, size_t size, size_t nmemb, void *userp) noexcept {
    std::string *destination = reinterpret_cast<std::string *>(userp);
    const size_t length = size * nmemb;
    destination->append(reinterpret_cast<char *>(buffer), length);
    return length;
}

static inline bool curlRequest(
        nlohmann::json &jsonResponse,
        const char *const requestType,
        const std::string &url,
        const std::string &apiToken,
        const std::string &fields
        ) noexcept
{
    std::string stringResponse;

    CURL *curlHandle = curl_easy_init();

    curl_easy_setopt(curlHandle, CURLOPT_CUSTOMREQUEST, requestType);
    curl_easy_setopt(curlHandle, CURLOPT_WRITEFUNCTION, writeCurlRequestResponse);
    curl_easy_setopt(curlHandle, CURLOPT_WRITEDATA, &stringResponse);
    curl_easy_setopt(curlHandle, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curlHandle, CURLOPT_TIMEOUT_MS, CURL_OPERATION_TIME_LIMIT_MS);

    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "accept: application/json");
    headers = curl_slist_append(headers, "content-type: application/json");
    headers = curl_slist_append(headers, ("authorization: Bearer " + apiToken).c_str());
    curl_easy_setopt(curlHandle, CURLOPT_HTTPHEADER, headers);

    if (!fields.empty()) {
        curl_easy_setopt(curlHandle, CURLOPT_POSTFIELDS, fields.c_str());
    }

    char curlErrorString[CURL_ERROR_SIZE];
    curl_easy_setopt(curlHandle, CURLOPT_ERRORBUFFER, curlErrorString);
    CURLcode curlError = curl_easy_perform(curlHandle);

    long httpStatus;
    curl_easy_getinfo(curlHandle, CURLINFO_RESPONSE_CODE, &httpStatus);

    curl_easy_cleanup(curlHandle);

    if (CURLE_OK != curlError) {
        std::cerr << "CURL failed with error (" << curlError << ") \"" << curlErrorString << "\"" << std::endl;
        return false;
    }
    if (httpStatus != 202 && httpStatus != 200) {
        std::cerr << "HTTP request failed with status " << httpStatus << std::endl;
        return false;
    }
    try {
        std::istringstream(stringResponse.c_str()) >> jsonResponse;
    } catch (nlohmann::json::exception &e) {
        std::cerr << "Invalid json returned from API." << std::endl;
        return false;
    }
    return true;
}

static inline bool waitOperation(
        nlohmann::json &jsonResponse,
        const std::string &apiToken
        ) noexcept
{
    const std::string operationId = jsonResponse["operation_id"];
    std::cout << "Waiting for operation " << operationId << " to terminate.............." << std::endl;
    do {
        jsonResponse.clear();
        if (!curlRequest(jsonResponse, "GET", "https://api.ubiquity.ai/v1/operations/" + operationId, apiToken, "")) {
            std::cerr << "API call failed." << std::endl;
            return false;
        }
        if (!jsonResponse["status"].is_string()) {
            std::cerr << "Invalid json returned from API." << std::endl;
            return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    } while (jsonResponse["status"] == "pending" || jsonResponse["status"] == "running");
    return jsonResponse["status"] == "succeeded";
}

std::string createModel(
        const std::string &modelType,
        const std::string &modelBody,
        const std::string &apiToken
        ) noexcept
{
    std::cout << "Creating a " << modelType << " model.............." << std::endl;
    nlohmann::json jsonResponse;
    if (!curlRequest(jsonResponse, "POST", "https://api.ubiquity.ai/v1/models/" + modelType, apiToken, modelBody)) {
        std::cerr << "API call failed." << std::endl;
        return "";
    }
    if (!jsonResponse["operation_id"].is_string()) {
        std::cerr << "Invalid json returned from API." << std::endl;
        return "";
    }
    if (!waitOperation(jsonResponse, apiToken)) {
        std::cerr << "Model creation operation failed." << std::endl;
        return "";
    }
    std::cout << "Model created successfully, generated modelId=" << jsonResponse["resource_id"] << std::endl;
    return jsonResponse["resource_id"];
}

void deleteModel(
        std::string modelId,
        const std::string &apiToken
        ) noexcept {
    std::cout << "Deleting a model with id=" << modelId << ".............." << std::endl;
    nlohmann::json jsonResponse;
    if (!curlRequest(jsonResponse, "DELETE", "https://api.ubiquity.ai/v1/models/" + modelId, apiToken, "")) {
        std::cerr << "API call failed." << std::endl;
        return;
    }
    if (!jsonResponse["operation_id"].is_string()) {
        std::cerr << "Invalid json returned from API." << std::endl;
        return;
    }
    if (!waitOperation(jsonResponse, apiToken)) {
        std::cerr << "Model deletion operation failed." << std::endl;
        return;
    }
    std::cout << "Model deleted successfully." << std::endl;
}

} // namespace Common

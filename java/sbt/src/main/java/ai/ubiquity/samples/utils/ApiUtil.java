package ai.ubiquity.samples.utils;

import okhttp3.*;
import org.json.JSONObject;

import java.io.IOException;

public class ApiUtil {
    private final String apiToken;
    private final OkHttpClient client;

    public ApiUtil(String apiToken) {
        this.apiToken = apiToken;
        this.client = new OkHttpClient();
    }

    public String createLineItemModel(String lineItemBody) throws IOException {
        System.out.println("Creating a line-item model...");
        String lineItemEndpoint = "https://api.ubiquity.ai/v1/models/line-item";
        return createModel(lineItemEndpoint, lineItemBody);
    }

    private String createModel(String endpoint, String modelBody) throws IOException {
        MediaType mediaType = MediaType.parse("application/json");
        RequestBody requestBody = RequestBody.create(modelBody, mediaType);
        Request request = new Request.Builder()
          .url(endpoint)
          .post(requestBody)
          .addHeader("accept", "application/json")
          .addHeader("content-type", "application/json")
          .addHeader("authorization", "Bearer " + apiToken)
          .build();

        String respBody;
        try (Response response = client.newCall(request).execute()) {
            int httpCode = response.code();
            if (httpCode != 202) {
                throw new RuntimeException("Create model request returned " + httpCode + " error code");
            }
            respBody = response.body().string();
        }
        JSONObject jsonObject = new JSONObject(respBody);
        String operationId = jsonObject.getString("operation_id");

        JSONObject operation = waitOperation(operationId);
        String modelId = operation.getString("resource_id");
        System.out.printf("Model created successfully, generated modelId=%s%n", modelId);
        return modelId;
    }

    public void deleteModel(String modelId) throws IOException {
        System.out.printf("Deleting a model with id=%s...%n", modelId);
        Request request = new Request.Builder()
          .url("https://api.ubiquity.ai/v1/models/" + modelId)
          .delete(null)
          .addHeader("accept", "application/json")
          .addHeader("authorization", "Bearer " + apiToken)
          .build();

        String respBody;
        try (Response response = client.newCall(request).execute()) {
            int httpCode = response.code();
            if (httpCode != 202) {
                throw new RuntimeException("Delete model request returned " + httpCode + " error code");
            }
            respBody = response.body().string();
        }
        JSONObject jsonObject = new JSONObject(respBody);

        String operation_id = jsonObject.getString("operation_id");
        JSONObject operation = waitOperation(operation_id);
        if (operation == null)
            throw new IllegalStateException("Failed to delete model");
        System.out.println("Model deleted successfully.");
    }

    private JSONObject waitOperation(String operationId) {
        String operationStatus;
        JSONObject operation;
        do {
            operation = getOperation(operationId);
            operationStatus = operation.getString("status");
            try {
                Thread.sleep(1000);
            } catch (InterruptedException e) {
                throw new RuntimeException(e);
            }
        } while (operationStatus.equals("pending") || operationStatus.equals("running"));
        return operationStatus.equals("succeeded") ? operation : null;
    }

    private JSONObject getOperation(String operationId) {
        Request request = new Request.Builder()
          .url("https://api.ubiquity.ai/v1/operations/" + operationId)
          .get()
          .addHeader("content-type", "application/json")
          .addHeader("accept", "application/json")
          .addHeader("authorization", "Bearer " + apiToken)
          .build();

        try {
            String respBody;
            try (Response response = client.newCall(request).execute()) {
                respBody = response.body().string();
            }
            return new JSONObject(respBody);
        } catch (IOException e) {
            throw new RuntimeException(e);
        }
    }
}

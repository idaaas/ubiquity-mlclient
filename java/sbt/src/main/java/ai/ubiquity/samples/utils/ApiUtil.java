package ai.ubiquity.samples.utils;

import okhttp3.*;
import org.json.JSONObject;

import java.io.IOException;

public class ApiUtil {
    private final String clusterId;
    private final String apiToken;
    private final OkHttpClient client;

    public ApiUtil(String clusterId, String apiToken) {
        this.clusterId = clusterId;
        this.apiToken = apiToken;
        this.client = new OkHttpClient();
    }

    public String createLineItemModel() throws IOException {
        System.out.println("Creating a line-item model...");
        String lineItemBody = getLineItemBody();
        String lineItemEndpoint = "https://api.ubiquity.ai/v1/models/line-item";
        return createModel(lineItemEndpoint, lineItemBody);
    }

    public String createBidShadingModel() throws IOException {
        System.out.println("Creating a bid-shading model...");
        String bidShadingBody = getBidShadingBody();
        String bidShadingEndpoint = "https://api.ubiquity.ai/v1/models/bid-shading";
        return createModel(bidShadingEndpoint, bidShadingBody);
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

    private String getLineItemBody() {
        return """
                {
                  "cluster_id": "%s",
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
                }""".formatted(clusterId);
    }

    private String getBidShadingBody() {
        return """
            {
               "cluster_id": "%s",
               "name": "code-example bid-shading",
               "min_bid_price": 0.0,
               "max_bid_price": 3.0,
               "feedback_delays": [
                 20,
                 30
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
            }""".formatted(clusterId);
    }
}

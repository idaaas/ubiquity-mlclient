using System;
using System.Net.Http;
using System.Net.Http.Headers;
using System.Text;
using System.Text.Json;
using System.Threading;
using Polly;

namespace ai.ubiquity.samples.common
{
    public sealed class ApiUtil : IDisposable
    {
        private const string BaseUrl = "https://api.ubiquity.ai/v1";
        private readonly HttpClient _client;
        private readonly Policy<HttpResponseMessage> _retryPolicy;

        public ApiUtil(string apiToken)
        {
            _client = new HttpClient
            {
                Timeout = TimeSpan.FromSeconds(10)
            };
            _client.DefaultRequestHeaders.Accept.Add(new MediaTypeWithQualityHeaderValue("application/json"));
            _client.DefaultRequestHeaders.Authorization = new AuthenticationHeaderValue("Bearer", apiToken);

            _retryPolicy = Policy<HttpResponseMessage>
                .Handle<TimeoutException>()
                .Or<TaskCanceledException>()
                .Or<HttpRequestException>(ex =>
                    ex.Message.Contains("timed out", StringComparison.OrdinalIgnoreCase) ||
                    ex.Message.Contains("A task was canceled", StringComparison.OrdinalIgnoreCase))
                .OrResult(response => (int)response.StatusCode >= 500)
                .WaitAndRetry(6, _ => TimeSpan.FromSeconds(1), onRetry: (outcome, timespan, retryAttempt, context) =>
                {
                    Console.WriteLine($"Request failed on attempt {retryAttempt}/6; retrying in {timespan.TotalSeconds:0} second(s)...");
                });
        }

        public string CreateLineItemModel(string lineItemBody)
        {
            Console.WriteLine("Creating a line-item model...");
            return CreateModel($"{BaseUrl}/models/line-item", lineItemBody);
        }

        public string CreateBidShadingModel(string bidShadingBody)
        {
            Console.WriteLine("Creating a bid-shading model...");
            return CreateModel($"{BaseUrl}/models/bid-shading", bidShadingBody);
        }

        public void DeleteModel(string modelId)
        {
            Console.WriteLine($"Deleting a model with id={modelId}...");

            using HttpResponseMessage response = _retryPolicy.Execute(() =>
            {
                using HttpRequestMessage request = new HttpRequestMessage(HttpMethod.Delete, $"{BaseUrl}/models/{modelId}");
                request.Headers.Accept.Add(new MediaTypeWithQualityHeaderValue("application/json"));
                return _client.Send(request);
            });
            string responseBody = response.Content.ReadAsStringAsync().GetAwaiter().GetResult();
            if ((int)response.StatusCode != 202)
            {
                throw new InvalidOperationException($"Delete model request returned {(int)response.StatusCode} error code");
            }

            using JsonDocument json = JsonDocument.Parse(responseBody);
            string operationId = json.RootElement.GetProperty("operation_id").GetString()
                ?? throw new InvalidOperationException("Delete model response does not contain operation_id");

            Operation? operation = WaitOperation(operationId);
            if (operation is null)
            {
                throw new InvalidOperationException("Failed to delete model");
            }

            Console.WriteLine("Model deleted successfully.");
        }

        public void Dispose()
        {
            _client.Dispose();
        }

        private string CreateModel(string endpoint, string modelBody)
        {
            using HttpResponseMessage response = _retryPolicy.Execute(() =>
            {
                using HttpRequestMessage request = new HttpRequestMessage(HttpMethod.Post, endpoint)
                {
                    Content = new StringContent(modelBody, Encoding.UTF8, "application/json")
                };
                request.Headers.Accept.Add(new MediaTypeWithQualityHeaderValue("application/json"));
                return _client.Send(request);
            });
            string responseBody = response.Content.ReadAsStringAsync().GetAwaiter().GetResult();
            if ((int)response.StatusCode != 202)
            {
                throw new InvalidOperationException($"Create model request returned {(int)response.StatusCode} error code");
            }

            using JsonDocument json = JsonDocument.Parse(responseBody);
            string operationId = json.RootElement.GetProperty("operation_id").GetString()
                ?? throw new InvalidOperationException("Create model response does not contain operation_id");

            Operation? operation = WaitOperation(operationId);
            if (operation is null || string.IsNullOrWhiteSpace(operation.ResourceId))
            {
                throw new InvalidOperationException("Model creation operation did not succeed");
            }

            Console.WriteLine($"Model created successfully, generated modelId={operation.ResourceId}");
            return operation.ResourceId;
        }

        private Operation? WaitOperation(string operationId)
        {
            while (true)
            {
                Operation operation = GetOperation(operationId);
                if (operation.Status == "pending" || operation.Status == "running")
                {
                    Thread.Sleep(1000);
                    continue;
                }

                return operation.Status == "succeeded" ? operation : null;
            }
        }

        private Operation GetOperation(string operationId)
        {
            using HttpResponseMessage response = _retryPolicy.Execute(() =>
            {
                using HttpRequestMessage request = new HttpRequestMessage(HttpMethod.Get, $"{BaseUrl}/operations/{operationId}");
                request.Headers.Accept.Add(new MediaTypeWithQualityHeaderValue("application/json"));
                return _client.Send(request);
            });
            string responseBody = response.Content.ReadAsStringAsync().GetAwaiter().GetResult();
            response.EnsureSuccessStatusCode();

            using JsonDocument json = JsonDocument.Parse(responseBody);
            JsonElement root = json.RootElement;
            string status = root.GetProperty("status").GetString()
                ?? throw new InvalidOperationException("Operation response does not contain status");
            string? resourceId = root.TryGetProperty("resource_id", out JsonElement resourceElement)
                ? resourceElement.GetString()
                : null;

            return new Operation(status, resourceId);
        }

        private sealed record Operation(string Status, string? ResourceId);
    }
}

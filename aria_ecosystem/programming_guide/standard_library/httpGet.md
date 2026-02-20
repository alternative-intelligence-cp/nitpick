# httpGet()

**Category**: Standard Library → HTTP  
**Syntax**: `httpGet(url: string) -> Result<HttpResponse>`  
**Purpose**: Make HTTP GET request

---

## Overview

`httpGet()` performs an HTTP GET request and returns the response.

---

## Syntax

```aria
import std.http;

response: Result<HttpResponse> = httpGet("https://api.example.com/data");
```

---

## Parameters

- **url** (`string`) - URL to fetch

---

## Returns

- `Result<HttpResponse>` - Response object on success, error on failure

---

## Examples

### Basic GET Request

```aria
import std.http;

Result: Result<HttpResponse> = httpGet("https://api.github.com/users/octocat");

when result is Ok(response) then
    stdout << "Status: $(response.status)";
    stdout << "Body: $(response.body)";
elsif result is Err(msg) then
    stderr << "Request failed: $msg";
end
```

### Parse JSON Response

```aria
response: HttpResponse = httpGet("https://api.example.com/users/1")?;

when response.status == 200 then
    user: obj = parse_json(response.body)?;
    stdout << "User: $(user.name)";
else
    stderr << "HTTP $(response.status): $(response.body)";
end
```

### Check Status Codes

```aria
response: HttpResponse = httpGet("https://example.com/api/data")?;

when response.status == 200 then
    stdout << "Success";
elsif response.status == 404 then
    stderr << "Not found";
elsif response.status >= 500 then
    stderr << "Server error";
else
    stderr << "Unexpected status: $(response.status)";
end
```

### Access Headers

```aria
response: HttpResponse = httpGet("https://example.com")?;

// Get specific header
content_type: ?string = response.headers.get("Content-Type");

// Print all headers
header_entries = response.headers.entries();
till(header_entries.length - 1, 1) {
    (key, value) = header_entries[$];
    stdout << "$key: $value";
end
```

---

## HttpResponse Structure

```aria
struct HttpResponse {
    status: i32,              // HTTP status code (200, 404, etc.)
    body: string,             // Response body
    headers: map<string, string>,  // Response headers
    status_text: string       // Status text ("OK", "Not Found", etc.)
}
```

---

## Error Cases

- Network error (no connection)
- DNS resolution failure
- Timeout
- Invalid URL
- TLS/SSL errors

---

## Best Practices

### ✅ DO: Check Status Codes

```aria
HttpResponse:response = httpGet(url)?;

when response.status != 200 then
    fail("HTTP $(response.status): $(response.status_text)");
end
```

### ✅ DO: Handle Network Errors

```aria
Result: Result<HttpResponse> = httpGet("https://unreliable-api.com");

when result is Err(msg) then
    log_error("Network error: $msg");
    // Retry logic or fallback
end
```

### ❌ DON'T: Ignore Response Codes

```aria
response: HttpResponse = httpGet(url)?;
data: obj = parse_json(response.body)?;  // ❌ What if 404?

// Better
when response.status == 200 then
    data: obj = parse_json(response.body)?;  // ✅
end
```

---

## With Query Parameters

```aria
// Build URL with parameters
base: string = "https://api.example.com/search";
query: string = "?q=aria&limit=10&offset=0";
url: string = base + query;

response: HttpResponse = httpGet(url)?;
```

---

## Related

- [http_client](http_client.md) - Full HTTP client (POST, PUT, DELETE)
- [httpPost()](httpPost.md) - HTTP POST requests
- [parse_json()](parse_json.md) - Parse JSON responses

---

**Remember**: Always **check status codes** before parsing response!

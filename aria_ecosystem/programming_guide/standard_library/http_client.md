# HTTP Client

**Category**: Standard Library → HTTP  
**Purpose**: Full-featured HTTP client for all request types

---

## Overview

The HTTP client provides comprehensive support for HTTP requests: GET, POST, PUT, DELETE, PATCH, etc.

---

## Importing

```aria
import std.http;
```

---

## Request Methods

### GET

```aria
response: HttpResponse = http.get("https://api.example.com/users")?;
```

### POST

```aria
body: string = to_json({ name = "Alice", age = 30 });

response: HttpResponse = http.post(
    "https://api.example.com/users",
    body,
    headers = { "Content-Type" = "application/json" }
)?;
```

### PUT

```aria
body: string = to_json({ name = "Alice Updated", age = 31 });

response: HttpResponse = http.put(
    "https://api.example.com/users/1",
    body,
    headers = { "Content-Type" = "application/json" }
)?;
```

### DELETE

```aria
response: HttpResponse = http.delete("https://api.example.com/users/1")?;
```

### PATCH

```aria
body: string = to_json({ age = 32 });

response: HttpResponse = http.patch(
    "https://api.example.com/users/1",
    body,
    headers = { "Content-Type" = "application/json" }
)?;
```

---

## Custom Headers

```aria
headers: map<string, string> = {
    "Authorization" = "Bearer $token",
    "Content-Type" = "application/json",
    "User-Agent" = "AriaApp/1.0"
};

response: HttpResponse = http.get(
    "https://api.example.com/protected",
    headers = headers
)?;
```

---

## Request Options

```aria
options: HttpOptions = {
    timeout = 30,          // Timeout in seconds
    follow_redirects = true,
    max_redirects = 5,
    verify_ssl = true
};

response: HttpResponse = http.get(url, options = options)?;
```

---

## REST API Example

```aria
struct User {
    id: i32,
    name: string,
    email: string
}

// GET - List users
fn get_users() -> Result<[]User> {
    response: HttpResponse = http.get("https://api.example.com/users")?;
    
    when response.status != 200 then
        return Err("Failed to fetch users");
    end
    
    data: obj = parse_json(response.body)?;
    return Ok(map_to_users(data));
}

// POST - Create user
fn create_user(name: string, email: string) -> Result<User> {
    body: string = to_json({ name = name, email = email });
    
    response: HttpResponse = http.post(
        "https://api.example.com/users",
        body,
        headers = { "Content-Type" = "application/json" }
    )?;
    
    when response.status != 201 then
        return Err("Failed to create user");
    end
    
    user_data: obj = parse_json(response.body)?;
    return Ok(map_to_user(user_data));
}

// PUT - Update user
fn update_user(id: i32, name: string, email: string) -> Result<User> {
    body: string = to_json({ name = name, email = email });
    
    response: HttpResponse = http.put(
        "https://api.example.com/users/$id",
        body,
        headers = { "Content-Type" = "application/json" }
    )?;
    
    when response.status != 200 then
        return Err("Failed to update user");
    end
    
    user_data: obj = parse_json(response.body)?;
    return Ok(map_to_user(user_data));
}

// DELETE - Remove user
fn delete_user(id: i32) -> Result<void> {
    response: HttpResponse = http.delete(
        "https://api.example.com/users/$id"
    )?;
    
    when response.status != 204 then
        return Err("Failed to delete user");
    end
    
    return Ok();
}
```

---

## File Upload

```aria
// Multipart form data
form: MultipartForm = {
    fields = {
        "name" = "profile.jpg",
        "description" = "Profile picture"
    },
    files = {
        "file" = read_bytes("profile.jpg")?
    }
};

response: HttpResponse = http.post_multipart(
    "https://api.example.com/upload",
    form
)?;
```

---

## Best Practices

### ✅ DO: Use Timeouts

```aria
options: HttpOptions = { timeout = 10 };
response: HttpResponse = http.get(url, options = options)?;
```

### ✅ DO: Validate Responses

```aria
response: HttpResponse = http.get(url)?;

when response.status < 200 or response.status >= 300 then
    return Err("HTTP $(response.status): $(response.status_text)");
end
```

### ✅ DO: Reuse Client for Multiple Requests

```aria
client: HttpClient = HttpClient.new();
defer client.close();

// Multiple requests with same client (connection pooling)
r1: HttpResponse = client.get("https://api.example.com/users")?;
r2: HttpResponse = client.get("https://api.example.com/posts")?;
```

---

## Related

- [httpGet()](httpGet.md) - Simple GET requests
- [readJSON()](readJSON.md) - Read JSON data

---

**Remember**: Use appropriate **HTTP methods** for REST operations!

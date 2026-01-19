# Structured Logging

**Category**: Standard Library → Logging  
**Purpose**: Best practices for structured, searchable logs

---

## Overview

**Structured logging** uses key-value pairs instead of plain text, making logs **machine-readable** and **searchable**.

---

## Text vs Structured

### ❌ Unstructured (Text)

```aria
stdout << "User alice logged in from 192.168.1.1 at 2025-12-27T10:30:00";
```

**Problems**:
- Hard to search for specific IPs
- Difficult to parse programmatically
- Inconsistent format

### ✅ Structured (Key-Value)

```aria
logger.info("User logged in", {
    user = "alice",
    ip = "192.168.1.1",
    timestamp = "2025-12-27T10:30:00"
});
```

**Benefits**:
- Easy to search: `ip="192.168.1.1"`
- Parse as JSON
- Consistent structure

---

## Basic Structured Logging

```aria
import std.log;

logger: Logger = createLogger("API");

// Log with structured context
logger.info("Request processed", {
    method = "GET",
    path = "/api/users",
    status = 200,
    duration_ms = 45
});
```

**Output (JSON)**:
```json
{
  "level": "info",
  "logger": "API",
  "msg": "Request processed",
  "method": "GET",
  "path": "/api/users",
  "status": 200,
  "duration_ms": 45,
  "timestamp": "2025-12-27T10:30:00Z"
}
```

---

## Common Patterns

### Request Logging

```aria
logger.info("HTTP request", {
    method = request.method,
    path = request.path,
    query = request.query_string,
    user_agent = request.headers.get("User-Agent"),
    ip = request.client_ip,
    status = response.status,
    duration_ms = elapsed_time
});
```

### Error Logging

```aria
logger.error("Database query failed", {
    query = "SELECT * FROM users WHERE id = ?",
    params = [user_id],
    error = error.message,
    error_code = error.code,
    retry_count = retries
});
```

### Business Events

```aria
logger.info("Order created", {
    order_id = order.id,
    customer_id = customer.id,
    amount = order.total,
    currency = "USD",
    items_count = order.items.length(),
    payment_method = order.payment_method
});
```

---

## Searchable Logs

With structured logs, you can easily query:

```bash
# Find all errors for specific user
jq 'select(.level=="error" and .user_id==12345)' app.log

# Find slow requests (> 1 second)
jq 'select(.duration_ms > 1000)' app.log

# Count requests by status code
jq '.status' app.log | sort | uniq -c
```

---

## Context Fields

### Always Include

```aria
{
    timestamp = now(),           // When
    level = "info",              // Severity
    logger = "ServiceName",      // Where
    msg = "What happened",       // Description
    // ... additional context
}
```

### Common Context

```aria
{
    user_id = user.id,          // Who
    request_id = uuid(),        // Trace requests
    session_id = session.id,    // Track sessions
    environment = "production", // Where deployed
    version = "1.2.3"           // Software version
}
```

---

## Best Practices

### ✅ DO: Use Structured Fields

```aria
// Good
logger.info("User login", {
    user_id = 123,
    username = "alice",
    success = true
});

// Bad - harder to search
logger.info("User alice (ID: 123) logged in successfully");
```

### ✅ DO: Use Consistent Field Names

```aria
// Consistent across logs
logger.info("Request started", { request_id = id });
logger.info("Database query", { request_id = id });
logger.info("Request completed", { request_id = id });

// Searchable: request_id="abc-123"
```

### ✅ DO: Include Request IDs for Tracing

```aria
request_id: string = uuid();

logger.info("Request received", { request_id = request_id });
logger.debug("Querying database", { request_id = request_id });
logger.info("Response sent", { request_id = request_id });

// All logs for one request can be traced
```

### ❌ DON'T: Log Sensitive Data

```aria
// BAD
logger.info("User data", {
    password = user.password,  // ❌ NEVER
    ssn = user.ssn,            // ❌ NEVER
    credit_card = payment.cc   // ❌ NEVER
});

// GOOD
logger.info("User authenticated", {
    user_id = user.id,  // ✅ Non-sensitive identifier
    method = "password" // ✅ Method only
});
```

---

## Log Aggregation

Structured logs work great with:

- **Elasticsearch**: Full-text search on JSON
- **Splunk**: Query structured fields
- **CloudWatch Logs**: Filter by fields
- **Datadog**: Metrics from log fields

```aria
// Automatically parseable
logger.info("Payment processed", {
    amount = 99.99,
    currency = "USD",
    gateway = "stripe",
    success = true
});
```

Query in aggregation tool:
```
success:true AND gateway:"stripe" AND amount:>50
```

---

## Correlation IDs

```aria
// Generate correlation ID at request start
correlation_id: string = uuid();

// Pass through entire request
logger.info("API request", { correlation_id = correlation_id });
db_query(sql, { correlation_id = correlation_id });
logger.info("API response", { correlation_id = correlation_id });

// All related logs share same correlation_id
```

---

## Related

- [createLogger()](createLogger.md) - Create logger
- [log_levels](log_levels.md) - Log levels

---

**Remember**: **Structured logs** are searchable and parseable!

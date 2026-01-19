# createLogger()

**Category**: Standard Library → Logging  
**Syntax**: `createLogger(name: string) -> Logger`  
**Purpose**: Create structured logger instance

---

## Overview

`createLogger()` creates a logger for structured, level-based logging.

---

## Syntax

```aria
import std.log;

logger: Logger = createLogger("MyApp");
```

---

## Parameters

- **name** (`string`) - Logger name (appears in log output)

---

## Returns

- `Logger` - Logger instance

---

## Log Levels

```aria
logger.debug("Detailed debugging info");
logger.info("General information");
logger.warn("Warning message");
logger.error("Error occurred");
logger.fatal("Critical failure");
```

---

## Examples

### Basic Logging

```aria
import std.log;

logger: Logger = createLogger("App");

logger.info("Application started");
logger.debug("Debug info: config loaded");
logger.warn("Low memory warning");
logger.error("Failed to connect to database");
```

### With Context

```aria
logger: Logger = createLogger("UserService");

logger.info("User logged in", {
    user_id = 12345,
    ip = "192.168.1.1",
    timestamp = now()
});
```

### Conditional Logging

```aria
logger: Logger = createLogger("API");

when response.status >= 400 then
    logger.error("HTTP error", {
        status = response.status,
        url = request.url,
        method = request.method
    });
end
```

---

## Structured Logging

```aria
logger: Logger = createLogger("PaymentService");

// Log with structured data
logger.info("Payment processed", {
    transaction_id = "TX123456",
    amount = 99.99,
    currency = "USD",
    customer_id = 789
});

// Output (JSON format):
// {"level":"info","logger":"PaymentService","msg":"Payment processed",
//  "transaction_id":"TX123456","amount":99.99,"currency":"USD","customer_id":789}
```

---

## Configuration

```aria
logger: Logger = createLogger("App");

// Set minimum level
logger.set_level(LogLevel.INFO);  // Only INFO and above

// Set output format
logger.set_format(LogFormat.JSON);  // or LogFormat.TEXT

// Set output destination
logger.set_output(stderr);  // or file handle
```

---

## Best Practices

### ✅ DO: Use Appropriate Levels

```aria
logger.debug("Variable value: $x");  // Development
logger.info("Request processed");    // Normal operation
logger.warn("Retry attempt 3/5");    // Potential issues
logger.error("Database timeout");    // Errors
logger.fatal("Cannot start");        // Critical failures
```

### ✅ DO: Add Context

```aria
logger.error("Operation failed", {
    operation = "save_user",
    user_id = user.id,
    error = error_msg
});
```

### ❌ DON'T: Log Sensitive Data

```aria
// BAD
logger.info("User data", {
    password = user.password  // ❌ NEVER log passwords!
});

// GOOD
logger.info("User authenticated", {
    user_id = user.id  // ✅ Log non-sensitive ID
});
```

---

## Multiple Loggers

```aria
// Different loggers for different modules
api_logger: Logger = createLogger("API");
db_logger: Logger = createLogger("Database");
auth_logger: Logger = createLogger("Auth");

api_logger.info("Request received");
db_logger.debug("Query executed");
auth_logger.warn("Login attempt failed");
```

---

## Related

- [log_levels](log_levels.md) - Log level details
- [structured_logging](structured_logging.md) - Structured logging guide

---

**Remember**: Use **structured logging** with context data!

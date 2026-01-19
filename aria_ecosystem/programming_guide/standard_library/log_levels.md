# Log Levels

**Category**: Standard Library → Logging  
**Purpose**: Understanding logging severity levels

---

## Overview

Log levels indicate the **severity** of log messages, from detailed debugging to critical errors.

---

## Levels (Lowest to Highest)

```aria
enum LogLevel {
    DEBUG,   // Detailed debugging information
    INFO,    // General informational messages
    WARN,    // Warning messages
    ERROR,   // Error messages
    FATAL    // Critical failures
}
```

---

## DEBUG

**Use for**: Detailed development/debugging information

```aria
logger.debug("Variable values: x=$x, y=$y, z=$z");
logger.debug("Entering function process_data()");
logger.debug("Cache hit for key: $key");
```

**Typically**: Disabled in production

---

## INFO

**Use for**: General informational messages about normal operation

```aria
logger.info("Application started");
logger.info("User logged in: $(user.name)");
logger.info("Request processed successfully");
logger.info("Configuration loaded from file");
```

**Typically**: Enabled in production for audit trail

---

## WARN

**Use for**: Warning messages - potential issues but not errors

```aria
logger.warn("Retry attempt 3/5");
logger.warn("Low disk space: $(free_space)MB remaining");
logger.warn("Deprecated function called");
logger.warn("Request took $(duration)ms (threshold: 1000ms)");
```

**Typically**: Always enabled, requires attention

---

## ERROR

**Use for**: Error conditions that don't stop the application

```aria
logger.error("Failed to connect to database");
logger.error("Invalid user input: $input");
logger.error("HTTP 500 error from external API");
logger.error("File not found: $path");
```

**Typically**: Always enabled, requires investigation

---

## FATAL

**Use for**: Critical errors that stop the application

```aria
logger.fatal("Cannot bind to port 8080");
logger.fatal("Required configuration missing");
logger.fatal("Out of memory");
logger.fatal("Database connection pool exhausted");
```

**Typically**: Always enabled, application usually exits after

---

## Setting Minimum Level

```aria
logger: Logger = createLogger("App");

// Only show WARN and above (WARN, ERROR, FATAL)
logger.set_level(LogLevel.WARN);

logger.debug("Not shown");  // Below threshold
logger.info("Not shown");   // Below threshold
logger.warn("Shown");       // At or above threshold
logger.error("Shown");      // At or above threshold
```

---

## Best Practices

### ✅ DO: Use Correct Level

```aria
// Normal operation
logger.info("Request completed");  // ✅

// Don't use error for normal events
logger.error("Request completed");  // ❌
```

### ✅ DO: DEBUG in Development, INFO in Production

```aria
when is_development() then
    logger.set_level(LogLevel.DEBUG);
else
    logger.set_level(LogLevel.INFO);
end
```

### ✅ DO: FATAL for Critical Issues Only

```aria
// Application cannot continue
logger.fatal("Cannot allocate memory");
exit(1);  // Application exits
```

---

## Level Comparison

```aria
| Level | Verbosity | Production | Use Case |
|-------|-----------|------------|----------|
| DEBUG | Highest   | ❌ No      | Development details |
| INFO  | High      | ✅ Yes     | Normal operations |
| WARN  | Medium    | ✅ Yes     | Potential issues |
| ERROR | Low       | ✅ Yes     | Actual errors |
| FATAL | Lowest    | ✅ Yes     | Critical failures |
```

---

## Environment-Based Configuration

```aria
fn setup_logging() -> Logger {
    logger: Logger = createLogger("App");
    
    env: string = get_env("ENVIRONMENT");
    
    when env == "development" then
        logger.set_level(LogLevel.DEBUG);
    elsif env == "staging" then
        logger.set_level(LogLevel.INFO);
    elsif env == "production" then
        logger.set_level(LogLevel.WARN);
    end
    
    return logger;
}
```

---

## Related

- [createLogger()](createLogger.md) - Create logger
- [structured_logging](structured_logging.md) - Structured logging

---

**Remember**: Choose the **right level** for each message!

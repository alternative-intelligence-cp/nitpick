# Common Patterns

**Category**: Advanced Features → Best Practices  
**Purpose**: Frequently used code patterns in Aria

---

## Overview

Common patterns that appear frequently in idiomatic Aria code.

---

## Builder Pattern

```aria
struct Config {
    host: string,
    port: i32,
    timeout: i32,
    retries: i32,
}

struct ConfigBuilder {
    host: ?string,
    port: ?i32,
    timeout: ?i32,
    retries: ?i32,
}

impl ConfigBuilder {
    pub fn new() -> ConfigBuilder {
        return ConfigBuilder {
            host: None,
            port: None,
            timeout: None,
            retries: None,
        };
    }
    
    pub fn host(host: string) -> ConfigBuilder {
        self.host = Some(host);
        return self;
    }
    
    pub fn port(port: i32) -> ConfigBuilder {
        self.port = Some(port);
        return self;
    }
    
    pub fn build() -> Config {
        return Config {
            host: self.host.unwrap_or("localhost"),
            port: self.port.unwrap_or(8080),
            timeout: self.timeout.unwrap_or(30),
            retries: self.retries.unwrap_or(3),
        };
    }
}

// Usage
config: Config = ConfigBuilder.new()
    .host("example.com")
    .port(443)
    .build();
```

---

## Option and Result Chaining

```aria
fn get_user_email(user_id: i32) -> ?string {
    return find_user(user_id)?
        .get_profile()?
        .email;
}

fn fetch_and_process() -> Result<Data> {
    return fetch_data()?
        .validate()?
        .transform()?
        .save()?;
}
```

---

## Iterator Pattern

```aria
fn process_users(users: []User) {
    users
        .filter(|u| u.age >= 18)
        .map(|u| u.name)
        .forEach(|name| {
            stdout << name;
        });
}

fn sum_even_squares(numbers: []i32) -> i32 {
    return numbers
        .filter(|n| n % 2 == 0)
        .map(|n| n * n)
        .sum();
}
```

---

## Factory Pattern

```aria
enum DatabaseType {
    Postgres,
    MySQL,
    SQLite,
}

trait Database {
    fn connect() -> Result<void>;
    fn query(sql: string) -> Result<[]Row>;
}

struct DatabaseFactory;

impl DatabaseFactory {
    pub fn create(db_type: DatabaseType) -> Box<dyn Database> {
        match db_type {
            DatabaseType.Postgres => return Box.new(PostgresDB.new()),
            DatabaseType.MySQL => return Box.new(MySQLDB.new()),
            DatabaseType.SQLite => return Box.new(SQLiteDB.new()),
        }
    }
}

// Usage
db: Box<dyn Database> = DatabaseFactory.create(DatabaseType.Postgres);
db.connect()?;
```

---

## Singleton Pattern

```aria
struct Logger {
    instance: ?*Logger = None,
}

impl Logger {
    pub fn get_instance() -> *Logger {
        if Logger.instance is None {
            Logger.instance = Some(alloc(Logger {
                // Initialize
            }));
        }
        return Logger.instance?;
    }
    
    pub fn log(message: string) {
        stdout << "[LOG] $message";
    }
}

// Usage
Logger.get_instance().log("Application started");
```

---

## Strategy Pattern

```aria
trait SortStrategy {
    fn sort(data: []i32) -> []i32;
}

struct QuickSort;
impl SortStrategy for QuickSort {
    fn sort(data: []i32) -> []i32 {
        // Quick sort implementation
    }
}

struct MergeSort;
impl SortStrategy for MergeSort {
    fn sort(data: []i32) -> []i32 {
        // Merge sort implementation
    }
}

struct Sorter {
    strategy: Box<dyn SortStrategy>,
}

impl Sorter {
    pub fn new(strategy: Box<dyn SortStrategy>) -> Sorter {
        return Sorter { strategy: strategy };
    }
    
    pub fn sort(data: []i32) -> []i32 {
        return self.strategy.sort(data);
    }
}

// Usage
sorter: Sorter = Sorter.new(Box.new(QuickSort));
sorted: []i32 = sorter.sort([3, 1, 4, 1, 5]);
```

---

## RAII Pattern

```aria
struct File {
    handle: *FileHandle,
}

impl File {
    pub fn open(path: string) -> Result<File> {
        handle: *FileHandle = open_file(path)?;
        return Ok(File { handle: handle });
    }
}

impl Drop for File {
    fn drop() {
        close_file(self.handle);
        stdout << "File closed automatically";
    }
}

// Usage
fn process() -> Result<void> {
    file: File = File.open("data.txt")?;
    // Use file
    return Ok();
}  // File automatically closed
```

---

## Newtype Pattern

```aria
struct UserId(i32);
struct ProductId(i32);

fn get_user(id: UserId) -> Result<User> {
    // Implementation
}

// Type safety!
user_id: UserId = UserId(42);
product_id: ProductId = ProductId(42);

get_user(user_id);         // ✅ Works
// get_user(product_id);   // ❌ Compile error
```

---

## Type State Pattern

```aria
struct Locked;
struct Unlocked;

struct Door<State> {
    state: State,
}

impl Door<Locked> {
    pub fn unlock() -> Door<Unlocked> {
        stdout << "Door unlocked";
        return Door { state: Unlocked };
    }
}

impl Door<Unlocked> {
    pub fn open() {
        stdout << "Door opened";
    }
    
    pub fn lock() -> Door<Locked> {
        stdout << "Door locked";
        return Door { state: Locked };
    }
}

// Usage
door: Door<Locked> = Door { state: Locked };
// door.open();              // ❌ Compile error - door is locked!
door = door.unlock();        // ✅ Unlock first
door.open();                 // ✅ Now can open
```

---

## Visitor Pattern

```aria
trait Expression {
    fn accept(visitor: *Visitor) -> i32;
}

struct Number {
    value: i32,
}

impl Expression for Number {
    fn accept(visitor: *Visitor) -> i32 {
        return visitor.visit_number(self);
    }
}

struct Add {
    left: Box<dyn Expression>,
    right: Box<dyn Expression>,
}

impl Expression for Add {
    fn accept(visitor: *Visitor) -> i32 {
        return visitor.visit_add(self);
    }
}

trait Visitor {
    fn visit_number(num: *Number) -> i32;
    fn visit_add(add: *Add) -> i32;
}

struct Evaluator;

impl Visitor for Evaluator {
    fn visit_number(num: *Number) -> i32 {
        return num.value;
    }
    
    fn visit_add(add: *Add) -> i32 {
        left: i32 = add.left.accept(self);
        right: i32 = add.right.accept(self);
        return left + right;
    }
}
```

---

## Command Pattern

```aria
trait Command {
    fn execute();
    fn undo();
}

struct AddUserCommand {
    user: User,
}

impl Command for AddUserCommand {
    fn execute() {
        database.insert(self.user);
    }
    
    fn undo() {
        database.delete(self.user.id);
    }
}

struct CommandHistory {
    history: []Box<dyn Command>,
}

impl CommandHistory {
    pub fn execute(cmd: Box<dyn Command>) {
        cmd.execute();
        self.history.push(cmd);
    }
    
    pub fn undo() {
        if let Some(cmd) = self.history.pop() {
            cmd.undo();
        }
    }
}
```

---

## Lazy Initialization

```aria
struct LazyValue<T> {
    value: ?T,
    initializer: fn() -> T,
}

impl<T> LazyValue<T> {
    pub fn new(initializer: fn() -> T) -> LazyValue<T> {
        return LazyValue {
            value: None,
            initializer: initializer,
        };
    }
    
    pub fn get() -> *T {
        if self.value is None {
            self.value = Some(self.initializer());
        }
        return &self.value?;
    }
}

// Usage
expensive_data: LazyValue<Vec<i32>> = LazyValue.new(|| {
    stdout << "Computing expensive data...";
    return compute_large_dataset();
});

// Not computed yet
// ...
// Computed on first access
data: *Vec<i32> = expensive_data.get();
```

---

## Related

- [best_practices](best_practices.md) - Best practices
- [idioms](idioms.md) - Aria idioms
- [code_examples](code_examples.md) - Code examples

---

**Remember**: Patterns are proven solutions to common problems - use them to write more maintainable code!

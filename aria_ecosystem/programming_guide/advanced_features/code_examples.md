# Code Examples

**Category**: Advanced Features → Best Practices  
**Purpose**: Complete code examples demonstrating Aria features

---

## Overview

Practical examples showing how to use Aria features in real-world scenarios.

---

## HTTP Server

```aria
import std.net.{TcpListener, TcpStream};
import std.io.{read, write};

struct Server {
    listener: TcpListener,
}

impl Server {
    pub fn new(port: i32) -> Result<Server> {
        listener: TcpListener = TcpListener.bind("0.0.0.0:$port")?;
        return Ok(Server { listener });
    }
    
    pub fn run() -> Result<void> {
        stdout << "Server listening on port ${self.listener.port()}";
        
        while true {
            stream: TcpStream = self.listener.accept()?;
            Thread.spawn(|| {
                self.handle_client(stream);
            });
        }
    }
    
    fn handle_client(stream: TcpStream) {
        defer stream.close();
        
        request: string = stream.read_to_string()?;
        response: string = self.process_request(request);
        stream.write_all(response.as_bytes())?;
    }
    
    fn process_request(request: string) -> string {
        if request.starts_with("GET /") {
            return "HTTP/1.1 200 OK\r\n\r\nHello, World!";
        }
        return "HTTP/1.1 404 Not Found\r\n\r\n404 Not Found";
    }
}

fn main() -> Result<void> {
    server: Server = Server.new(8080)?;
    server.run()?;
    return Ok();
}
```

---

## JSON Parser

```aria
enum Json {
    Null,
    Bool(bool),
    Number(f64),
    String(string),
    Array([]Json),
    Object(HashMap<string, Json>),
}

struct JsonParser {
    input: string,
    pos: i32,
}

impl JsonParser {
    pub fn parse(input: string) -> Result<Json> {
        parser: JsonParser = JsonParser { input, pos: 0 };
        return parser.parse_value();
    }
    
    fn parse_value() -> Result<Json> {
        self.skip_whitespace();
        
        c: char = self.peek()?;
        
        match c {
            'n' => return self.parse_null(),
            't' | 'f' => return self.parse_bool(),
            '"' => return self.parse_string(),
            '[' => return self.parse_array(),
            '{' => return self.parse_object(),
            _ if c.is_digit() || c == '-' => return self.parse_number(),
            _ => return Err("Unexpected character"),
        }
    }
    
    fn parse_string() -> Result<Json> {
        self.expect('"')?;
        start: i32 = self.pos;
        
        while self.peek()? != '"' {
            if self.peek()? == '\\' {
                self.advance();  // Skip escape
            }
            self.advance();
        }
        
        value: string = self.input[start..self.pos];
        self.expect('"')?;
        
        return Ok(Json.String(value));
    }
    
    fn parse_array() -> Result<Json> {
        self.expect('[')?;
        items: []Json = [];
        
        self.skip_whitespace();
        if self.peek()? == ']' {
            self.advance();
            return Ok(Json.Array(items));
        }
        
        while true {
            item: Json = self.parse_value()?;
            items.push(item);
            
            self.skip_whitespace();
            if self.peek()? == ']' {
                self.advance();
                break;
            }
            
            self.expect(',')?;
        }
        
        return Ok(Json.Array(items));
    }
}

// Usage
json: Json = JsonParser.parse('{"name": "Alice", "age": 30}')?;
```

---

## Database Connection Pool

```aria
import std.sync.{Mutex, Semaphore};

struct Connection {
    id: i32,
    // Connection details
}

impl Connection {
    fn execute(query: string) -> Result<[]Row> {
        // Execute query
    }
}

struct ConnectionPool {
    connections: Mutex<[]Connection>,
    semaphore: Semaphore,
    max_size: i32,
}

impl ConnectionPool {
    pub fn new(max_size: i32) -> ConnectionPool {
        connections: []Connection = [];
        
        till(max_size - 1, 1) {
            connections.push(Connection { id: $ });
        }
        
        return ConnectionPool {
            connections: Mutex.new(connections),
            semaphore: Semaphore.new(max_size),
            max_size: max_size,
        };
    }
    
    pub fn acquire() -> Result<Connection> {
        // Wait for available connection
        self.semaphore.acquire();
        
        // Get connection from pool
        lock = self.connections.lock();
        conn: Connection = lock.pop()?;
        
        return Ok(conn);
    }
    
    pub fn release(conn: Connection) {
        // Return connection to pool
        lock = self.connections.lock();
        lock.push(conn);
        
        // Signal availability
        self.semaphore.release();
    }
}

// Usage
pool: ConnectionPool = ConnectionPool.new(10);

fn query_database() -> Result<[]Row> {
    conn: Connection = pool.acquire()?;
    defer pool.release(conn);
    
    results: []Row = conn.execute("SELECT * FROM users")?;
    return Ok(results);
}
```

---

## File Watcher

```aria
import std.fs.{watch, Event, EventKind};
import std.path.Path;

struct FileWatcher {
    path: Path,
    handlers: HashMap<EventKind, fn(Event)>,
}

impl FileWatcher {
    pub fn new(path: Path) -> FileWatcher {
        return FileWatcher {
            path: path,
            handlers: HashMap.new(),
        };
    }
    
    pub fn on(kind: EventKind, handler: fn(Event)) {
        self.handlers.insert(kind, handler);
    }
    
    pub fn start() -> Result<void> {
        watcher = watch(self.path)?;
        
        while event = watcher.recv()? {
            if let Some(handler) = self.handlers.get(event.kind) {
                handler(event);
            }
        }
        
        return Ok();
    }
}

// Usage
fn main() -> Result<void> {
    watcher: FileWatcher = FileWatcher.new(Path.new("./watched"));
    
    watcher.on(EventKind.Create, |event| {
        stdout << "File created: ${event.path}";
    });
    
    watcher.on(EventKind.Modify, |event| {
        stdout << "File modified: ${event.path}";
    });
    
    watcher.on(EventKind.Delete, |event| {
        stdout << "File deleted: ${event.path}";
    });
    
    watcher.start()?;
    return Ok();
}
```

---

## Command Line Parser

```aria
struct Args {
    program: string,
    flags: HashMap<string, bool>,
    options: HashMap<string, string>,
    positional: []string,
}

impl Args {
    pub fn parse(args: []string) -> Args {
        Result: Args = Args {
            program: args[0],
            flags: HashMap.new(),
            options: HashMap.new(),
            positional: [],
        };
        
        i: i32 = 1;
        while i < args.len() {
            arg: string = args[i];
            
            if arg.starts_with("--") {
                // Long option
                if arg.contains("=") {
                    parts: []string = arg[2..].split("=");
                    result.options.insert(parts[0], parts[1]);
                } else {
                    if i + 1 < args.len() && !args[i + 1].starts_with("-") {
                        result.options.insert(arg[2..], args[i + 1]);
                        i += 1;
                    } else {
                        result.flags.insert(arg[2..], true);
                    }
                }
            } else if arg.starts_with("-") {
                // Short flag
                result.flags.insert(arg[1..], true);
            } else {
                // Positional argument
                result.positional.push(arg);
            }
            
            i += 1;
        }
        
        return result;
    }
    
    pub fn get_flag(name: string) -> bool {
        return self.flags.get(name).unwrap_or(false);
    }
    
    pub fn get_option(name: string) -> ?string {
        return self.options.get(name);
    }
}

// Usage
fn main() {
    args: Args = Args.parse(std.env.args());
    
    if args.get_flag("help") {
        print_help();
        return;
    }
    
    port: i32 = args.get_option("port")
        .unwrap_or("8080")
        .parse()?;
    
    till(args.positional.length - 1, 1) {
        process_file(args.positional[$]);
    }
}
```

---

## Async HTTP Client

```aria
import std.http.{Request, Response};

struct HttpClient {
    base_url: string,
    timeout: i32,
}

impl HttpClient {
    pub fn new(base_url: string) -> HttpClient {
        return HttpClient {
            base_url: base_url,
            timeout: 30,
        };
    }
    
    pub async fn get(path: string) -> Result<Response> {
        url: string = "${self.base_url}$path";
        
        request: Request = Request.builder()
            .url(url)
            .method("GET")
            .timeout(self.timeout)
            .build();
        
        response: Response = await request.send()?;
        
        if !response.ok() {
            return Err("HTTP ${response.status}");
        }
        
        return Ok(response);
    }
    
    pub async fn post(path: string, body: string) -> Result<Response> {
        url: string = "${self.base_url}$path";
        
        request: Request = Request.builder()
            .url(url)
            .method("POST")
            .header("Content-Type", "application/json")
            .body(body)
            .build();
        
        response: Response = await request.send()?;
        return Ok(response);
    }
}

// Usage
async fn main() -> Result<void> {
    client: HttpClient = HttpClient.new("https://api.example.com");
    
    response: Response = await client.get("/users")?;
    users: []User = await response.json()?;
    
    till(users.length - 1, 1) {
        stdout << users[$].name;
    }
    
    return Ok();
}
```

---

## Related

- [best_practices](best_practices.md) - Best practices
- [common_patterns](common_patterns.md) - Common patterns
- [idioms](idioms.md) - Aria idioms

---

**Remember**: These examples demonstrate real-world usage of Aria features!

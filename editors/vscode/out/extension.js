"use strict";
var __createBinding = (this && this.__createBinding) || (Object.create ? (function(o, m, k, k2) {
    if (k2 === undefined) k2 = k;
    var desc = Object.getOwnPropertyDescriptor(m, k);
    if (!desc || ("get" in desc ? !m.__esModule : desc.writable || desc.configurable)) {
      desc = { enumerable: true, get: function() { return m[k]; } };
    }
    Object.defineProperty(o, k2, desc);
}) : (function(o, m, k, k2) {
    if (k2 === undefined) k2 = k;
    o[k2] = m[k];
}));
var __setModuleDefault = (this && this.__setModuleDefault) || (Object.create ? (function(o, v) {
    Object.defineProperty(o, "default", { enumerable: true, value: v });
}) : function(o, v) {
    o["default"] = v;
});
var __importStar = (this && this.__importStar) || (function () {
    var ownKeys = function(o) {
        ownKeys = Object.getOwnPropertyNames || function (o) {
            var ar = [];
            for (var k in o) if (Object.prototype.hasOwnProperty.call(o, k)) ar[ar.length] = k;
            return ar;
        };
        return ownKeys(o);
    };
    return function (mod) {
        if (mod && mod.__esModule) return mod;
        var result = {};
        if (mod != null) for (var k = ownKeys(mod), i = 0; i < k.length; i++) if (k[i] !== "default") __createBinding(result, mod, k[i]);
        __setModuleDefault(result, mod);
        return result;
    };
})();
Object.defineProperty(exports, "__esModule", { value: true });
exports.activate = activate;
exports.deactivate = deactivate;
const fs = __importStar(require("fs"));
const path = __importStar(require("path"));
const vscode_1 = require("vscode");
const node_1 = require("vscode-languageclient/node");
let client;
function activate(context) {
    console.log('Aria language extension activating...');
    // Get server path from configuration or use bundled binary
    const serverPath = getServerPath(context);
    if (!serverPath || !fs.existsSync(serverPath)) {
        vscode_1.window.showErrorMessage(`Aria language server not found at: ${serverPath}. ` +
            'Please install the Aria compiler or set aria.server.path in settings.');
        return;
    }
    // Server options: run aria-ls in stdio mode
    const serverOptions = {
        run: {
            command: serverPath,
            transport: node_1.TransportKind.stdio,
            args: vscode_1.workspace.getConfiguration('aria').get('server.args', [])
        },
        debug: {
            command: serverPath,
            transport: node_1.TransportKind.stdio,
            args: vscode_1.workspace.getConfiguration('aria').get('server.args', []),
            options: {
                env: {
                    ...process.env,
                    RUST_LOG: 'debug' // If we add logging
                }
            }
        }
    };
    // Client options: document selector for .aria files
    const clientOptions = {
        documentSelector: [
            { scheme: 'file', language: 'aria' },
            { scheme: 'untitled', language: 'aria' }
        ],
        synchronize: {
            // Notify server of aria.toml changes
            fileEvents: vscode_1.workspace.createFileSystemWatcher('**/aria.toml')
        },
        initializationOptions: {
        // Can pass custom initialization data here
        }
    };
    // Create and start the language client
    client = new node_1.LanguageClient('ariaLanguageServer', 'Aria Language Server', serverOptions, clientOptions);
    // Start the client (will also launch the server)
    client.start();
    console.log('Aria language server started');
}
function deactivate() {
    if (!client) {
        return undefined;
    }
    return client.stop();
}
/**
 * Get the path to aria-ls executable
 *
 * Priority:
 * 1. User-configured path (aria.server.path setting)
 * 2. Bundled binary (platform-specific)
 * 3. System PATH
 */
function getServerPath(context) {
    // Check user configuration first
    const configPath = vscode_1.workspace.getConfiguration('aria').get('server.path');
    if (configPath && configPath.trim() !== '') {
        return configPath;
    }
    // Try bundled binary
    const bundledPath = getBundledServerPath(context);
    if (bundledPath && fs.existsSync(bundledPath)) {
        return bundledPath;
    }
    // Try system PATH
    const systemPath = findInPath('aria-ls');
    if (systemPath) {
        return systemPath;
    }
    return undefined;
}
/**
 * Get path to bundled aria-ls binary based on platform
 */
function getBundledServerPath(context) {
    const platform = process.platform;
    const arch = process.arch;
    let binaryName = 'aria-ls';
    let subdir = '';
    if (platform === 'win32') {
        binaryName = 'aria-ls.exe';
        subdir = 'windows';
    }
    else if (platform === 'darwin') {
        subdir = 'macos';
    }
    else if (platform === 'linux') {
        subdir = 'linux';
    }
    else {
        return undefined;
    }
    // Path: extension-root/bin/<platform>/aria-ls
    const serverPath = path.join(context.extensionPath, 'bin', subdir, binaryName);
    return serverPath;
}
/**
 * Search for executable in system PATH
 */
function findInPath(executable) {
    const envPath = process.env.PATH || '';
    const pathDirs = envPath.split(path.delimiter);
    for (const dir of pathDirs) {
        const fullPath = path.join(dir, executable);
        if (fs.existsSync(fullPath)) {
            return fullPath;
        }
        // Try with .exe on Windows
        if (process.platform === 'win32') {
            const exePath = fullPath + '.exe';
            if (fs.existsSync(exePath)) {
                return exePath;
            }
        }
    }
    return undefined;
}
//# sourceMappingURL=extension.js.map
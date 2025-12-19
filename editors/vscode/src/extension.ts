import * as fs from 'fs';
import * as path from 'path';
import { ExtensionContext, window, workspace } from 'vscode';
import {
    LanguageClient,
    LanguageClientOptions,
    ServerOptions,
    TransportKind
} from 'vscode-languageclient/node';

let client: LanguageClient | undefined;

export function activate(context: ExtensionContext) {
    console.log('Aria language extension activating...');

    // Get server path from configuration or use bundled binary
    const serverPath = getServerPath(context);

    if (!serverPath || !fs.existsSync(serverPath)) {
        window.showErrorMessage(
            `Aria language server not found at: ${serverPath}. ` +
            'Please install the Aria compiler or set aria.server.path in settings.'
        );
        return;
    }

    // Server options: run aria-ls in stdio mode
    const serverOptions: ServerOptions = {
        run: {
            command: serverPath,
            transport: TransportKind.stdio,
            args: workspace.getConfiguration('aria').get('server.args', [])
        },
        debug: {
            command: serverPath,
            transport: TransportKind.stdio,
            args: workspace.getConfiguration('aria').get('server.args', []),
            options: {
                env: {
                    ...process.env,
                    RUST_LOG: 'debug'  // If we add logging
                }
            }
        }
    };

    // Client options: document selector for .aria files
    const clientOptions: LanguageClientOptions = {
        documentSelector: [
            { scheme: 'file', language: 'aria' },
            { scheme: 'untitled', language: 'aria' }
        ],
        synchronize: {
            // Notify server of aria.toml changes
            fileEvents: workspace.createFileSystemWatcher('**/aria.toml')
        },
        initializationOptions: {
            // Can pass custom initialization data here
        }
    };

    // Create and start the language client
    client = new LanguageClient(
        'ariaLanguageServer',
        'Aria Language Server',
        serverOptions,
        clientOptions
    );

    // Start the client (will also launch the server)
    client.start();

    console.log('Aria language server started');
}

export function deactivate(): Thenable<void> | undefined {
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
function getServerPath(context: ExtensionContext): string | undefined {
    // Check user configuration first
    const configPath = workspace.getConfiguration('aria').get<string>('server.path');
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
function getBundledServerPath(context: ExtensionContext): string | undefined {
    const platform = process.platform;
    const arch = process.arch;

    let binaryName = 'aria-ls';
    let subdir = '';

    if (platform === 'win32') {
        binaryName = 'aria-ls.exe';
        subdir = 'windows';
    } else if (platform === 'darwin') {
        subdir = 'macos';
    } else if (platform === 'linux') {
        subdir = 'linux';
    } else {
        return undefined;
    }

    // Path: extension-root/bin/<platform>/aria-ls
    const serverPath = path.join(
        context.extensionPath,
        'bin',
        subdir,
        binaryName
    );

    return serverPath;
}

/**
 * Search for executable in system PATH
 */
function findInPath(executable: string): string | undefined {
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

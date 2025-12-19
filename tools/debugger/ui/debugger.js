/**
 * Aria Debugger UI - Client-Side Logic
 * Handles WebSocket communication with DAP server and UI updates
 */

class AriaDebugger {
    constructor() {
        this.ws = null;
        this.connected = false;
        this.currentFile = null;
        this.currentLine = null;
        this.breakpoints = new Map(); // file -> Set<line>
        this.variables = [];
        this.callStack = [];

        this.initializeUI();
        this.connectWebSocket();
    }

    initializeUI() {
        // Control buttons
        document.getElementById('btn-continue').addEventListener('click', () => this.continue());
        document.getElementById('btn-pause').addEventListener('click', () => this.pause());
        document.getElementById('btn-step-over').addEventListener('click', () => this.stepOver());
        document.getElementById('btn-step-into').addEventListener('click', () => this.stepInto());
        document.getElementById('btn-step-out').addEventListener('click', () => this.stepOut());
        document.getElementById('btn-restart').addEventListener('click', () => this.restart());
        document.getElementById('btn-stop').addEventListener('click', () => this.stop());

        // Console
        document.getElementById('btn-clear-console').addEventListener('click', () => this.clearConsole());
        document.getElementById('console-input').addEventListener('keypress', (e) => {
            if (e.key === 'Enter') {
                this.evaluateExpression(e.target.value);
                e.target.value = '';
            }
        });

        // Keyboard shortcuts
        document.addEventListener('keydown', (e) => {
            if (e.key === 'F5') {
                e.preventDefault();
                if (e.shiftKey) this.restart();
                else this.continue();
            } else if (e.key === 'F10') {
                e.preventDefault();
                this.stepOver();
            } else if (e.key === 'F11') {
                e.preventDefault();
                if (e.shiftKey) this.stepOut();
                else this.stepInto();
            }
        });
    }

    connectWebSocket() {
        const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
        const wsUrl = `${protocol}//${window.location.host}/ws`;

        this.log(`Connecting to ${wsUrl}...`, 'info');

        this.ws = new WebSocket(wsUrl);

        this.ws.onopen = () => {
            this.connected = true;
            this.updateConnectionStatus(true);
            this.log('Connected to debugger', 'success');
            this.enableControls(false); // Enabled when debugging starts
        };

        this.ws.onclose = () => {
            this.connected = false;
            this.updateConnectionStatus(false);
            this.log('Disconnected from debugger', 'error');
            this.enableControls(false);

            // Reconnect after 3 seconds
            setTimeout(() => this.connectWebSocket(), 3000);
        };

        this.ws.onerror = (error) => {
            this.log('WebSocket error', 'error');
            console.error('WebSocket error:', error);
        };

        this.ws.onmessage = (event) => {
            this.handleMessage(JSON.parse(event.data));
        };
    }

    handleMessage(message) {
        console.log('Received:', message);

        if (message.type === 'event') {
            this.handleEvent(message);
        } else if (message.type === 'response') {
            this.handleResponse(message);
        }
    }

    handleEvent(event) {
        switch (event.event) {
            case 'initialized':
                this.log('Debug session initialized', 'success');
                break;

            case 'stopped':
                this.handleStopped(event.body);
                break;

            case 'continued':
                this.handleContinued();
                break;

            case 'exited':
                this.handleExited(event.body);
                break;

            case 'terminated':
                this.handleTerminated();
                break;

            case 'breakpoint':
                this.handleBreakpointEvent(event.body);
                break;
        }
    }

    handleStopped(body) {
        const reason = body.reason || 'unknown';
        this.log(`Stopped: ${reason}`, 'info');
        this.enableControls(true);

        // Request stack trace
        this.sendRequest('stackTrace', {
            threadId: body.threadId
        }, (response) => {
            if (response.success) {
                this.updateCallStack(response.body.stackFrames);
            }
        });
    }

    handleContinued() {
        this.log('Continuing execution...', 'info');
        this.enableControls(false);
        this.enableControls(true, ['btn-pause', 'btn-stop']);
    }

    handleExited(body) {
        const code = body.exitCode || 0;
        this.log(`Program exited with code ${code}`, code === 0 ? 'success' : 'error');
        this.enableControls(false);
    }

    handleTerminated() {
        this.log('Debug session terminated', 'info');
        this.enableControls(false);
        this.clearState();
    }

    handleBreakpointEvent(body) {
        if (body.reason === 'changed') {
            this.log(`Breakpoint ${body.breakpoint.id} ${body.breakpoint.verified ? 'verified' : 'unverified'}`, 'info');
        }
    }

    handleResponse(response) {
        // Response handling is done via callbacks in sendRequest
    }

    // ========================================================================
    // DAP Commands
    // ========================================================================

    continue() {
        this.sendRequest('continue', { threadId: 1 });
    }

    pause() {
        this.sendRequest('pause', { threadId: 1 });
    }

    stepOver() {
        this.sendRequest('next', { threadId: 1 });
    }

    stepInto() {
        this.sendRequest('stepIn', { threadId: 1 });
    }

    stepOut() {
        this.sendRequest('stepOut', { threadId: 1 });
    }

    restart() {
        this.sendRequest('restart', {});
    }

    stop() {
        this.sendRequest('disconnect', {});
    }

    evaluateExpression(expr) {
        if (!expr.trim()) return;

        this.log(`> ${expr}`, 'info');

        this.sendRequest('evaluate', {
            expression: expr,
            frameId: 0,
            context: 'repl'
        }, (response) => {
            if (response.success) {
                this.log(`${response.body.result}`, 'success');
            } else {
                this.log(`Error: ${response.message}`, 'error');
            }
        });
    }

    toggleBreakpoint(file, line) {
        if (!this.breakpoints.has(file)) {
            this.breakpoints.set(file, new Set());
        }

        const fileBreakpoints = this.breakpoints.get(file);

        if (fileBreakpoints.has(line)) {
            fileBreakpoints.delete(line);
        } else {
            fileBreakpoints.add(line);
        }

        // Send to server
        this.sendRequest('setBreakpoints', {
            source: { path: file },
            breakpoints: Array.from(fileBreakpoints).map(l => ({ line: l }))
        });

        this.updateBreakpointsList();
    }

    // ========================================================================
    // UI Updates
    // ========================================================================

    updateConnectionStatus(connected) {
        const status = document.getElementById('connection-status');
        if (connected) {
            status.textContent = '● Connected';
            status.className = 'status-connected';
        } else {
            status.textContent = '● Disconnected';
            status.className = 'status-disconnected';
        }
    }

    enableControls(enabled, only = null) {
        const buttons = only || [
            'btn-continue', 'btn-pause', 'btn-step-over',
            'btn-step-into', 'btn-step-out', 'btn-restart', 'btn-stop'
        ];

        buttons.forEach(id => {
            document.getElementById(id).disabled = !enabled;
        });

        document.getElementById('console-input').disabled = !enabled;
    }

    updateCallStack(frames) {
        this.callStack = frames;
        const container = document.getElementById('call-stack');

        if (frames.length === 0) {
            container.innerHTML = '<div class="placeholder">No call stack</div>';
            return;
        }

        container.innerHTML = frames.map((frame, index) => `
            <div class="call-stack-frame ${index === 0 ? 'active' : ''}" 
                 onclick="debugger.selectFrame(${frame.id})">
                <div class="frame-function">${this.escapeHtml(frame.name)}</div>
                <div class="frame-location">${this.escapeHtml(frame.source?.name || 'unknown')}:${frame.line}</div>
            </div>
        `).join('');

        // Update source view for first frame
        if (frames[0] && frames[0].source) {
            this.loadSourceFile(frames[0].source.path, frames[0].line);
        }

        // Request variables for first frame
        this.requestVariables(frames[0].id);
    }

    selectFrame(frameId) {
        // Update active frame UI
        document.querySelectorAll('.call-stack-frame').forEach((el, index) => {
            el.classList.toggle('active', index === frameId);
        });

        const frame = this.callStack[frameId];
        if (frame && frame.source) {
            this.loadSourceFile(frame.source.path, frame.line);
        }

        this.requestVariables(frameId);
    }

    requestVariables(frameId) {
        // Request scopes for frame
        this.sendRequest('scopes', { frameId }, (response) => {
            if (response.success && response.body.scopes.length > 0) {
                // Request variables for first scope (locals)
                const scope = response.body.scopes[0];
                this.sendRequest('variables', {
                    variablesReference: scope.variablesReference
                }, (varResponse) => {
                    if (varResponse.success) {
                        this.updateVariables(varResponse.body.variables);
                    }
                });
            }
        });
    }

    updateVariables(variables) {
        this.variables = variables;
        const container = document.getElementById('variables');

        if (variables.length === 0) {
            container.innerHTML = '<div class="placeholder">No variables in scope</div>';
            return;
        }

        container.innerHTML = variables.map(v => this.renderVariable(v)).join('');
    }

    renderVariable(variable, indent = 0) {
        const hasChildren = variable.variablesReference > 0;
        const style = `padding-left: ${indent * 20}px`;

        let html = `<div class="variable ${hasChildren ? 'variable-expandable' : ''}" style="${style}">`;
        html += `<span class="variable-name">${this.escapeHtml(variable.name)}</span>: `;
        html += `<span class="variable-value">${this.escapeHtml(variable.value)}</span>`;
        html += `<span class="variable-type">${this.escapeHtml(variable.type)}</span>`;
        html += `</div>`;

        return html;
    }

    loadSourceFile(path, highlightLine) {
        this.currentFile = path;
        this.currentLine = highlightLine;

        document.getElementById('current-file').textContent = path.split('/').pop();

        // In a real implementation, we would fetch the file content
        // For now, show a placeholder
        const sourceView = document.getElementById('source-view');
        sourceView.innerHTML = `
            <div class="source-placeholder">
                <p>📄 ${this.escapeHtml(path)}</p>
                <p>Line ${highlightLine}</p>
                <p style="margin-top: 20px; font-size: 12px;">
                    Source file loading requires HTTP server implementation
                </p>
            </div>
        `;
    }

    updateBreakpointsList() {
        const container = document.getElementById('breakpoints');
        const allBreakpoints = [];

        for (const [file, lines] of this.breakpoints.entries()) {
            for (const line of lines) {
                allBreakpoints.push({ file, line });
            }
        }

        if (allBreakpoints.length === 0) {
            container.innerHTML = '<div class="placeholder">No breakpoints set</div>';
            return;
        }

        container.innerHTML = allBreakpoints.map((bp, index) => `
            <div class="breakpoint">
                <div class="breakpoint-location">
                    ${this.escapeHtml(bp.file.split('/').pop())}:${bp.line}
                </div>
                <button class="breakpoint-remove" 
                        onclick="debugger.toggleBreakpoint('${bp.file}', ${bp.line})">
                    ×
                </button>
            </div>
        `).join('');
    }

    clearState() {
        this.currentFile = null;
        this.currentLine = null;
        this.callStack = [];
        this.variables = [];

        document.getElementById('call-stack').innerHTML = '<div class="placeholder">No active debugging session</div>';
        document.getElementById('variables').innerHTML = '<div class="placeholder">No variables in scope</div>';

        const sourceView = document.getElementById('source-view');
        sourceView.innerHTML = `
            <div class="source-placeholder">
                <p>📄 No source file loaded</p>
                <p>Start debugging to see source code</p>
            </div>
        `;
    }

    // ========================================================================
    // Console
    // ========================================================================

    log(message, type = 'info') {
        const output = document.getElementById('console-output');
        const div = document.createElement('div');
        div.className = `console-message ${type}`;
        div.textContent = `[${new Date().toLocaleTimeString()}] ${message}`;
        output.appendChild(div);
        output.scrollTop = output.scrollHeight;
    }

    clearConsole() {
        document.getElementById('console-output').innerHTML = '';
    }

    // ========================================================================
    // Helpers
    // ========================================================================

    sendRequest(command, args, callback) {
        if (!this.connected) {
            this.log('Not connected to debugger', 'error');
            return;
        }

        const request = {
            seq: Date.now(),
            type: 'request',
            command,
            arguments: args
        };

        console.log('Sending:', request);
        this.ws.send(JSON.stringify(request));

        // Store callback for response handling
        // (In a real implementation, match by seq number)
        if (callback) {
            this.pendingCallbacks = this.pendingCallbacks || {};
            this.pendingCallbacks[request.seq] = callback;
        }
    }

    escapeHtml(text) {
        const div = document.createElement('div');
        div.textContent = text;
        return div.innerHTML;
    }
}

// Initialize debugger when page loads
let debugger;
window.addEventListener('DOMContentLoaded', () => {
    debugger = new AriaDebugger();

    // Log initial message
    debugger.log('Aria Debugger UI loaded', 'success');
    debugger.log('Waiting for WebSocket connection...', 'info');
    debugger.log('Note: Web UI is under development - use VS Code integration for full debugging', 'info');
});

/**
 * Aria Memory Map Visualizer - JavaScript
 * Phase 7.4.6: Interactive Memory Layout Viewer
 * 
 * Canvas-based memory visualization with:
 * - Grid layout for memory blocks
 * - Color-coded region types
 * - Interactive zoom/pan
 * - Hover tooltips with object headers
 * - Click for hex dump
 * - GC statistics overlay
 * - Fragmentation heatmap
 */

class MemoryMapVisualizer {
    constructor() {
        // Canvas and context
        this.canvas = document.getElementById('memory-canvas');
        this.ctx = this.canvas.getContext('2d');

        // Memory map data
        this.memoryMap = null;
        this.regions = [];
        this.blocks = [];
        this.statistics = {};

        // View state
        this.zoom = 1.0;
        this.panX = 0;
        this.panY = 0;
        this.isDragging = false;
        this.lastMouseX = 0;
        this.lastMouseY = 0;

        // Filter settings
        this.filterType = 'all';
        this.showFragmentation = true;
        this.showStats = true;

        // Layout parameters
        this.blockSize = 10; // pixels per block
        this.blocksPerRow = 64; // Grid layout

        // Initialize
        this.setupCanvas();
        this.setupEventListeners();
        this.updateStatus('Ready. Click "Refresh Memory Map" to scan process memory.');
    }

    setupCanvas() {
        // Set canvas size to match container
        const container = this.canvas.parentElement;
        this.canvas.width = container.clientWidth;
        this.canvas.height = 600;

        // Handle high-DPI displays
        const dpr = window.devicePixelRatio || 1;
        this.canvas.style.width = this.canvas.width + 'px';
        this.canvas.style.height = this.canvas.height + 'px';
        this.canvas.width *= dpr;
        this.canvas.height *= dpr;
        this.ctx.scale(dpr, dpr);
    }

    setupEventListeners() {
        // Refresh button
        document.getElementById('refresh-btn').addEventListener('click', () => {
            this.refreshMemoryMap();
        });

        // Reset view button
        document.getElementById('reset-view-btn').addEventListener('click', () => {
            this.resetView();
        });

        // Filter type
        document.getElementById('filter-type').addEventListener('change', (e) => {
            this.filterType = e.target.value;
            this.render();
        });

        // Zoom control
        document.getElementById('zoom-level').addEventListener('input', (e) => {
            this.zoom = parseFloat(e.target.value);
            document.getElementById('zoom-display').textContent = this.zoom.toFixed(1) + 'x';
            this.render();
        });

        // Fragmentation toggle
        document.getElementById('show-fragmentation').addEventListener('change', (e) => {
            this.showFragmentation = e.target.checked;
            this.render();
        });

        // Stats toggle
        document.getElementById('show-stats').addEventListener('change', (e) => {
            this.showStats = e.target.checked;
            const statsPanel = document.getElementById('statistics-panel');
            statsPanel.style.display = e.target.checked ? 'block' : 'none';
        });

        // Canvas mouse events
        this.canvas.addEventListener('mousedown', (e) => this.handleMouseDown(e));
        this.canvas.addEventListener('mousemove', (e) => this.handleMouseMove(e));
        this.canvas.addEventListener('mouseup', (e) => this.handleMouseUp(e));
        this.canvas.addEventListener('mouseleave', (e) => this.handleMouseLeave(e));
        this.canvas.addEventListener('click', (e) => this.handleClick(e));

        // Modal close handlers
        document.querySelectorAll('.modal-close').forEach(closeBtn => {
            closeBtn.addEventListener('click', (e) => {
                e.target.closest('.modal').classList.remove('visible');
            });
        });

        // Close modals on background click
        document.querySelectorAll('.modal').forEach(modal => {
            modal.addEventListener('click', (e) => {
                if (e.target === modal) {
                    modal.classList.remove('visible');
                }
            });
        });

        // Window resize
        window.addEventListener('resize', () => {
            this.setupCanvas();
            this.render();
        });
    }

    async refreshMemoryMap() {
        this.updateStatus('Scanning process memory...');

        try {
            // In a real DAP integration, this would be a WebSocket message
            // For now, simulate with mock data
            const response = await this.sendDAPRequest('ariaMemoryMap', {
                type: this.filterType
            });

            if (response.success) {
                this.memoryMap = response.body;
                this.regions = response.body.regions || [];
                this.blocks = response.body.blocks || [];
                this.statistics = response.body.statistics || {};

                this.updateStatistics();
                this.render();
                this.updateStatus(`Memory map loaded: ${this.regions.length} regions, ${this.blocks.length} blocks`);
            } else {
                this.updateStatus('Error: ' + response.message, true);
            }
        } catch (error) {
            this.updateStatus('Failed to load memory map: ' + error.message, true);

            // Load mock data for demonstration
            this.loadMockData();
        }
    }

    async sendDAPRequest(command, args) {
        // This would be implemented as WebSocket communication in production
        // For now, return mock response
        throw new Error('DAP connection not available (mock mode)');
    }

    loadMockData() {
        // Mock data for demonstration purposes
        this.updateStatus('Using mock data (DAP connection not available)');

        // Generate mock regions
        this.regions = [
            {
                start: 0x7f0000000000,
                end: 0x7f0000100000,
                size: 1048576,
                type: 'gc_nursery',
                name: 'GC Nursery',
                object_count: 1523,
                fragmentation: 0.12
            },
            {
                start: 0x7f0000100000,
                end: 0x7f0000400000,
                size: 3145728,
                type: 'gc_old_gen',
                name: 'GC Old Generation',
                object_count: 8942,
                fragmentation: 0.24
            },
            {
                start: 0x7f0000400000,
                end: 0x7f0000500000,
                size: 1048576,
                type: 'wild',
                name: 'Wild Heap',
                object_count: 0,
                fragmentation: 0.05
            },
            {
                start: 0x7f0000500000,
                end: 0x7f0000510000,
                size: 65536,
                type: 'wildx',
                name: 'WildX (JIT Code)',
                object_count: 0,
                fragmentation: 0.0
            }
        ];

        // Generate mock blocks
        this.blocks = [];
        let blockId = 0;
        for (const region of this.regions) {
            const numBlocks = Math.floor(region.size / 4096);
            for (let i = 0; i < Math.min(numBlocks, 100); i++) {
                this.blocks.push({
                    address: region.start + (i * 4096),
                    size: 4096,
                    type: region.type,
                    is_allocated: Math.random() > 0.3,
                    tooltip: `Address: 0x${(region.start + (i * 4096)).toString(16)}\nSize: 4096 bytes\nType: ${region.type}`
                });
            }
        }

        // Mock statistics
        this.statistics = {
            nursery_size: 1048576,
            nursery_used: 923648,
            old_gen_size: 3145728,
            old_gen_used: 2391296,
            total_allocations: 10465,
            total_collections: 42,
            nursery_collections: 38,
            old_gen_collections: 4,
            fragmentation_percent: 18.5,
            live_objects: 10465,
            total_bytes_allocated: 52428800,
            total_bytes_freed: 48283136
        };

        this.updateStatistics();
        this.render();
    }

    updateStatistics() {
        const formatBytes = (bytes) => {
            if (bytes === 0) return '0 B';
            const k = 1024;
            const sizes = ['B', 'KB', 'MB', 'GB'];
            const i = Math.floor(Math.log(bytes) / Math.log(k));
            return (bytes / Math.pow(k, i)).toFixed(2) + ' ' + sizes[i];
        };

        document.getElementById('stat-nursery-size').textContent = formatBytes(this.statistics.nursery_size || 0);
        document.getElementById('stat-nursery-used').textContent = formatBytes(this.statistics.nursery_used || 0);
        document.getElementById('stat-old-gen-size').textContent = formatBytes(this.statistics.old_gen_size || 0);
        document.getElementById('stat-old-gen-used').textContent = formatBytes(this.statistics.old_gen_used || 0);
        document.getElementById('stat-total-allocs').textContent = (this.statistics.total_allocations || 0).toLocaleString();
        document.getElementById('stat-total-collections').textContent = (this.statistics.total_collections || 0).toLocaleString();
        document.getElementById('stat-nursery-collections').textContent = (this.statistics.nursery_collections || 0).toLocaleString();
        document.getElementById('stat-old-gen-collections').textContent = (this.statistics.old_gen_collections || 0).toLocaleString();
        document.getElementById('stat-fragmentation').textContent = (this.statistics.fragmentation_percent || 0).toFixed(1) + '%';
        document.getElementById('stat-live-objects').textContent = (this.statistics.live_objects || 0).toLocaleString();
        document.getElementById('stat-bytes-allocated').textContent = formatBytes(this.statistics.total_bytes_allocated || 0);
        document.getElementById('stat-bytes-freed').textContent = formatBytes(this.statistics.total_bytes_freed || 0);
    }

    render() {
        const ctx = this.ctx;
        const width = this.canvas.width / (window.devicePixelRatio || 1);
        const height = this.canvas.height / (window.devicePixelRatio || 1);

        // Clear canvas
        ctx.clearRect(0, 0, width, height);
        ctx.fillStyle = '#f9fafb';
        ctx.fillRect(0, 0, width, height);

        // Save context state
        ctx.save();

        // Apply zoom and pan transformations
        ctx.translate(this.panX, this.panY);
        ctx.scale(this.zoom, this.zoom);

        // Render blocks in grid layout
        this.renderBlocks(ctx);

        // Restore context state
        ctx.restore();
    }

    renderBlocks(ctx) {
        const blockSize = this.blockSize;
        const blocksPerRow = this.blocksPerRow;

        // Color mapping
        const colorMap = {
            gc_nursery: '#4ade80',
            gc_old_gen: '#60a5fa',
            wild: '#9ca3af',
            wildx: '#f87171',
            free: '#f3f4f6',
            unknown: '#374151'
        };

        this.blocks.forEach((block, index) => {
            const row = Math.floor(index / blocksPerRow);
            const col = index % blocksPerRow;
            const x = col * (blockSize + 1);
            const y = row * (blockSize + 1);

            // Get color based on type
            let color = colorMap[block.type] || colorMap.unknown;

            // Apply fragmentation heatmap overlay
            if (this.showFragmentation && block.is_allocated) {
                // Darken allocated blocks slightly
                ctx.fillStyle = this.darkenColor(color, 0.8);
            } else if (this.showFragmentation && !block.is_allocated) {
                // Lighten unallocated blocks
                ctx.fillStyle = this.lightenColor(color, 1.2);
            } else {
                ctx.fillStyle = color;
            }

            // Draw block
            ctx.fillRect(x, y, blockSize, blockSize);

            // Draw border for better visibility
            ctx.strokeStyle = '#e5e7eb';
            ctx.lineWidth = 0.5;
            ctx.strokeRect(x, y, blockSize, blockSize);
        });
    }

    darkenColor(color, factor) {
        const r = parseInt(color.slice(1, 3), 16);
        const g = parseInt(color.slice(3, 5), 16);
        const b = parseInt(color.slice(5, 7), 16);
        return `rgb(${Math.floor(r * factor)}, ${Math.floor(g * factor)}, ${Math.floor(b * factor)})`;
    }

    lightenColor(color, factor) {
        const r = parseInt(color.slice(1, 3), 16);
        const g = parseInt(color.slice(3, 5), 16);
        const b = parseInt(color.slice(5, 7), 16);
        return `rgb(${Math.min(255, Math.floor(r * factor))}, ${Math.min(255, Math.floor(g * factor))}, ${Math.min(255, Math.floor(b * factor))})`;
    }

    handleMouseDown(e) {
        this.isDragging = true;
        this.lastMouseX = e.clientX;
        this.lastMouseY = e.clientY;
        this.canvas.style.cursor = 'grabbing';
    }

    handleMouseMove(e) {
        if (this.isDragging) {
            const dx = e.clientX - this.lastMouseX;
            const dy = e.clientY - this.lastMouseY;
            this.panX += dx;
            this.panY += dy;
            this.lastMouseX = e.clientX;
            this.lastMouseY = e.clientY;
            this.render();
        } else {
            // Show tooltip on hover
            const block = this.getBlockAtPoint(e.clientX, e.clientY);
            if (block) {
                this.showTooltip(e.clientX, e.clientY, block.tooltip);
            } else {
                this.hideTooltip();
            }
        }
    }

    handleMouseUp(e) {
        this.isDragging = false;
        this.canvas.style.cursor = 'crosshair';
    }

    handleMouseLeave(e) {
        this.isDragging = false;
        this.canvas.style.cursor = 'crosshair';
        this.hideTooltip();
    }

    handleClick(e) {
        const block = this.getBlockAtPoint(e.clientX, e.clientY);
        if (block) {
            // Show hex dump modal
            this.showHexDumpModal(block);
        }
    }

    getBlockAtPoint(clientX, clientY) {
        const rect = this.canvas.getBoundingClientRect();
        const x = (clientX - rect.left - this.panX) / this.zoom;
        const y = (clientY - rect.top - this.panY) / this.zoom;

        const blockSize = this.blockSize + 1;
        const col = Math.floor(x / blockSize);
        const row = Math.floor(y / blockSize);
        const index = row * this.blocksPerRow + col;

        if (index >= 0 && index < this.blocks.length) {
            return this.blocks[index];
        }
        return null;
    }

    showTooltip(x, y, text) {
        const tooltip = document.getElementById('tooltip');
        tooltip.textContent = text;
        tooltip.style.left = (x + 15) + 'px';
        tooltip.style.top = (y + 15) + 'px';
        tooltip.classList.add('visible');
    }

    hideTooltip() {
        const tooltip = document.getElementById('tooltip');
        tooltip.classList.remove('visible');
    }

    showHexDumpModal(block) {
        // Populate modal with block info
        document.getElementById('hex-address').textContent = '0x' + block.address.toString(16);
        document.getElementById('hex-size').textContent = block.size + ' bytes';
        document.getElementById('hex-type').textContent = block.type;

        // Generate mock hex dump (in production, would request from DAP)
        const hexDump = this.generateMockHexDump(block.address, block.size);
        document.getElementById('hex-dump').innerHTML = hexDump;

        // Show modal
        document.getElementById('hex-modal').classList.add('visible');
    }

    generateMockHexDump(address, size) {
        let html = '';
        const bytesPerLine = 16;
        const numLines = Math.min(16, Math.ceil(size / bytesPerLine));

        for (let i = 0; i < numLines; i++) {
            const lineAddr = address + (i * bytesPerLine);
            html += `<div>0x${lineAddr.toString(16).padStart(16, '0')}:  `;

            // Hex bytes
            for (let j = 0; j < bytesPerLine; j++) {
                const byte = Math.floor(Math.random() * 256);
                html += byte.toString(16).padStart(2, '0') + ' ';
            }

            html += ' |';

            // ASCII representation
            for (let j = 0; j < bytesPerLine; j++) {
                const byte = Math.floor(Math.random() * 256);
                const char = (byte >= 32 && byte < 127) ? String.fromCharCode(byte) : '.';
                html += char;
            }

            html += '|</div>';
        }

        return html;
    }

    resetView() {
        this.zoom = 1.0;
        this.panX = 0;
        this.panY = 0;
        document.getElementById('zoom-level').value = 1.0;
        document.getElementById('zoom-display').textContent = '1.0x';
        this.render();
        this.updateStatus('View reset');
    }

    updateStatus(message, isError = false) {
        const statusElement = document.getElementById('status-message');
        statusElement.textContent = message;
        statusElement.style.color = isError ? '#ef4444' : '#111827';
    }
}

// Initialize visualizer when DOM is loaded
document.addEventListener('DOMContentLoaded', () => {
    window.memoryVisualizer = new MemoryMapVisualizer();

    // Auto-load mock data for demonstration
    setTimeout(() => {
        window.memoryVisualizer.loadMockData();
    }, 500);
});

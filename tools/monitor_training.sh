#!/bin/bash
# Aria Specialist Training Monitor
# Usage: ./monitor_training.sh

echo "==================================================================="
echo "Aria Language Specialist - Training Monitor"
echo "==================================================================="
echo ""

# Check if process is running
if ps aux | grep -q "[t]rain_aria_specialist"; then
    echo "✓ Training Status: RUNNING"
    PID=$(ps aux | grep "[t]rain_aria_specialist" | awk '{print $2}')
    RUNTIME=$(ps -p $PID -o etime= | tr -d ' ')
    echo "  PID: $PID"
    echo "  Runtime: $RUNTIME"
else
    echo "✗ Training Status: NOT RUNNING"
    if [ -f "aria_specialist_model/pytorch_model.bin" ] || [ -f "aria_specialist_model/adapter_model.bin" ]; then
        echo "  (Training may have completed)"
    fi
fi

echo ""
echo "-------------------------------------------------------------------"
echo "GPU Status:"
nvidia-smi --query-gpu=index,name,utilization.gpu,utilization.memory,memory.used,memory.total,temperature.gpu --format=csv | sed 's/,/│/g'
echo ""

echo "-------------------------------------------------------------------"
echo "Recent Training Log (last 20 lines):"
echo ""
if [ -f "training.log" ]; then
    tail -20 training.log
else
    echo "  No training.log found yet"
fi

echo ""
echo "-------------------------------------------------------------------"
echo "Checkpoints:"
if [ -d "aria_specialist_model" ]; then
    ls -lh aria_specialist_model/ | grep -E "(checkpoint|adapter_model)" | awk '{print "  " $9 " (" $5 ")"}'
    
    # Count checkpoints
    CHECKPOINT_COUNT=$(ls aria_specialist_model/ | grep -c "checkpoint" || echo "0")
    echo ""
    echo "  Total checkpoints saved: $CHECKPOINT_COUNT"
else
    echo "  No checkpoints yet"
fi

echo ""
echo "==================================================================="
echo "Commands:"
echo "  Watch progress:  watch -n 30 ./monitor_training.sh"
echo "  View full log:   less training.log"
echo "  Stop training:   kill $PID"
echo "==================================================================="

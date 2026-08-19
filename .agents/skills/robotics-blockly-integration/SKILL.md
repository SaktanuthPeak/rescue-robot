---
name: robotics-blockly-integration
description: Google Blockly workspace integration in Svelte 5, custom robotics blocks definition (Movement, LiDAR, IR, NeoPixel, Buzzer), dual code generation (Robot Command Sequence & C++ Arduino Transpiler), and execution engine.
---

# Google Blockly & Robotics Transpiler Skill

This skill provides patterns for building the visual block-based programming interface with **Google Blockly** inside **Svelte 5**, generating both robot runtime commands and live Arduino C++ code for education.

---

## 1. Svelte 5 Blockly Workspace Container

Use standard Svelte 5 component lifecycle with runes and element binding:

```svelte
<script lang="ts">
    import { onMount, onDestroy } from 'svelte';
    import * as Blockly from 'blockly';
    import { javascriptGenerator } from 'blockly/javascript';
    import { defineRoboticsBlocks } from '$lib/blockly/blocks';
    import { registerCppGenerator } from '$lib/blockly/cpp_generator';

    let { onCodeChange, onCommandsGenerated } = $props<{
        onCodeChange?: (cppCode: string) => void;
        onCommandsGenerated?: (commands: any[]) => void;
    }>();

    let blocklyDiv: HTMLDivElement;
    let workspace: Blockly.WorkspaceSvg;

    onMount(() => {
        defineRoboticsBlocks();
        registerCppGenerator();

        workspace = Blockly.inject(blocklyDiv, {
            toolbox: document.getElementById('toolbox') as HTMLElement,
            grid: { spacing: 20, length: 3, colour: '#ccc', snap: true },
            zoom: { controls: true, wheel: true, startScale: 1.0, maxScale: 3, minScale: 0.3, scaleSpeed: 1.2 },
            trashcan: true,
            renderer: 'zelos'
        });

        workspace.addChangeListener((event) => {
            if (event.isUiEvent) return;
            const cppCode = (Blockly as any).Arduino.workspaceToCode(workspace);
            onCodeChange?.(cppCode);
        });
    });

    onDestroy(() => {
        if (workspace) workspace.dispose();
    });
</script>

<div class="w-full h-full relative">
    <div bind:this={blocklyDiv} class="w-full h-full"></div>
</div>
```

---

## 2. Defining Custom Robotics Blocks

```typescript
import * as Blockly from 'blockly';

export function defineRoboticsBlocks() {
    // 1. Move Distance Block
    Blockly.Blocks['robot_move'] = {
        init: function() {
            this.appendDummyInput()
                .appendField("🚶 เดินหน้า / ถอยหลัง")
                .appendField(new Blockly.FieldDropdown([
                    ["เดินหน้า", "FORWARD"],
                    ["ถอยหลัง", "BACKWARD"]
                ]), "DIRECTION")
                .appendField(new Blockly.FieldNumber(20, -500, 500), "DISTANCE")
                .appendField("ซม. ที่ความเร็ว")
                .appendField(new Blockly.FieldNumber(70, 0, 100), "SPEED")
                .appendField("%");
            this.setPreviousStatement(true, null);
            this.setNextStatement(true, null);
            this.setColour(230);
            this.setTooltip("เคลื่อนที่ตามระยะทางที่กำหนดเป็นเซนติเมตร");
        }
    };

    // 2. Turn Angle Block
    Blockly.Blocks['robot_turn'] = {
        init: function() {
            this.appendDummyInput()
                .appendField("🔄 เลี้ยว")
                .appendField(new Blockly.FieldDropdown([
                    ["ขวา", "RIGHT"],
                    ["ซ้าย", "LEFT"]
                ]), "DIRECTION")
                .appendField(new Blockly.FieldNumber(90, 0, 360), "ANGLE")
                .appendField("องศา");
            this.setPreviousStatement(true, null);
            this.setNextStatement(true, null);
            this.setColour(230);
            this.setTooltip("หมุนตัวหุ่นยนต์ตามองศาที่กำหนด");
        }
    };

    // 3. Read LiDAR Distance
    Blockly.Blocks['robot_read_lidar'] = {
        init: function() {
            this.appendDummyInput()
                .appendField("📏 อ่านระยะทางข้างหน้า (LiDAR)");
            this.setOutput(true, "Number");
            this.setColour(160);
            this.setTooltip("คืนค่าระยะห่างด้านหน้าเป็นเซนติเมตร (0-1200 ซม.)");
        }
    };

    // 4. NeoPixel RGB Color
    Blockly.Blocks['robot_set_rgb'] = {
        init: function() {
            this.appendDummyInput()
                .appendField("💡 เปลี่ยนสีไฟ LED")
                .appendField(new Blockly.FieldColour("#ff0000"), "COLOR");
            this.setPreviousStatement(true, null);
            this.setNextStatement(true, null);
            this.setColour(60);
        }
    };

    // 5. Buzzer Tone
    Blockly.Blocks['robot_buzzer_note'] = {
        init: function() {
            this.appendDummyInput()
                .appendField("🔔 ส่งเสียงโน้ต")
                .appendField(new Blockly.FieldDropdown([
                    ["C4 (โด)", "262"],
                    ["D4 (เร)", "294"],
                    ["E4 (มี)", "330"],
                    ["F4 (ฟา)", "349"],
                    ["G4 (ซอล)", "392"],
                    ["A4 (ลา)", "440"],
                    ["B4 (ที)", "494"],
                    ["C5 (โดสูง)", "523"]
                ]), "NOTE")
                .appendField("นาน")
                .appendField(new Blockly.FieldNumber(300, 50, 5000), "DURATION")
                .appendField("มิลลิวินาที");
            this.setPreviousStatement(true, null);
            this.setNextStatement(true, null);
            this.setColour(60);
        }
    };
}
```

---

## 3. Real-time C++ Arduino Transpiler

Generates authentic, clean Arduino C++ code that students can study:

```typescript
export function registerCppGenerator() {
    const Arduino = new Blockly.Generator('Arduino');

    Arduino['robot_move'] = function(block: Blockly.Block) {
        const dir = block.getFieldValue('DIRECTION');
        let dist = block.getFieldValue('DISTANCE');
        const speed = block.getFieldValue('SPEED');
        if (dir === 'BACKWARD') dist = -dist;
        return `robot.move(${dist}, ${speed});\n`;
    };

    Arduino['robot_turn'] = function(block: Blockly.Block) {
        const dir = block.getFieldValue('DIRECTION');
        let angle = block.getFieldValue('ANGLE');
        if (dir === 'LEFT') angle = -angle;
        return `robot.turn(${angle});\n`;
    };

    Arduino['robot_read_lidar'] = function(block: Blockly.Block) {
        return ['robot.getLidarDistance()', Arduino.ORDER_ATOMIC];
    };

    Arduino['robot_set_rgb'] = function(block: Blockly.Block) {
        const hex = block.getFieldValue('COLOR');
        // Parse hex to r,g,b
        const r = parseInt(hex.slice(1, 3), 16);
        const g = parseInt(hex.slice(3, 5), 16);
        const b = parseInt(hex.slice(5, 7), 16);
        return `robot.setRGB(${r}, ${g}, ${b});\n`;
    };

    Arduino['robot_buzzer_note'] = function(block: Blockly.Block) {
        const note = block.getFieldValue('NOTE');
        const dur = block.getFieldValue('DURATION');
        return `robot.playTone(${note}, ${dur});\n`;
    };

    (Blockly as any).Arduino = Arduino;
}
```

---

## 4. Execution Pipeline (Visual Code -> Execution)

1. **Serialize to Commands:** The visual block tree is evaluated into an async sequence of commands.
2. **WebSocket Dispatch:** Commands are dispatched sequentially or as a script package via `/ws/execute` to FastAPI backend.
3. **Execution Acknowledgements:** As each step finishes (e.g. ESP32 reaches target encoder distance and emits `{"t":"ACK","cmd":"MOVE","ok":true}`), the frontend highlights the active executing block on the canvas.

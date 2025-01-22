### Custom GstAggregator Requirements Document

#### **Overview**
This document outlines the requirements, constraints, functional and non-functional specifications, and use cases for a custom `GstAggregator` element designed for video analytics with metadata processing in NVIDIA DeepStream pipelines.

---

### **1. Core Problem**

1. **Primary Input**:
   - The **primary sink pad** receives a video stream in **nvmm** format.
   - Video frames may include **metadata** (`NvDsBatchMeta`) that must be preserved in the output.

2. **Secondary Inputs**:
   - The **secondary sink pads** receive video frames (also in **nvmm** format) with `NvDsBatchMeta` metadata attached.
   - Metadata includes object detection, classifications, tracking information, or other analytics.

3. **Output Requirements**:
   - The output stream consists of the primary video stream with combined metadata from the primary and secondary inputs.
   - Metadata must align with the corresponding video frames based on timestamps.

4. **Capabilities**:
   - All pads use **nvmm** format.
   - Pads may have different **resolutions** and **color spaces**.

---

### **2. Key Constraints**

1. **Synchronization**:
   - Frame-perfect synchronization between the primary and all secondary sink pads is required.
   - Use the default `next_time` behavior provided by `GstAggregator` for timestamp alignment.

2. **Metadata Handling**:
   - Metadata from the primary input must be preserved in the output.
   - Metadata from secondary inputs must be merged into the primary frame’s metadata.
   - Logging:
     - Log an **info** message if no buffer arrives on a secondary sink pad.
     - Log a **warning** message if a buffer arrives on a secondary sink pad but contains no metadata.

3. **Caps and Format Consistency**:
   - All pads use **nvmm** format.
   - Pads may differ in resolution and color space.
   - The final output caps must be the **common caps** negotiated between the primary sink pad and downstream elements.

4. **Error Handling**:
   - Missing buffers or metadata on secondary inputs must not disrupt the pipeline.
   - The pipeline must proceed with available buffers and metadata.

5. **Performance**:
   - Real-time synchronization and processing are required to maintain pipeline throughput.

---

### **3. Functional Requirements**

1. **Inputs**:
   - **Primary sink pad**: Receives a video stream with optional metadata.
   - **Secondary sink pads**: Receive frames with attached metadata (`NvDsBatchMeta`).

2. **Processing**:
   - Extract metadata from secondary sink pads.
   - Merge metadata into the primary frame’s metadata.
   - Synchronize all input buffers using timestamp alignment.

3. **Outputs**:
   - Forward the primary video stream with merged metadata from secondary inputs.
   - Ensure compatibility with downstream elements (e.g., NVIDIA DeepStream).

4. **Logging and Error Handling**:
   - Log missing buffers or metadata as specified in the constraints.
   - Continue processing even if secondary inputs are incomplete or inconsistent.

5. **Real-Time Performance**:
   - Maintain minimal latency and high throughput for real-time video analytics.

---

### **4. Non-Functional Requirements**

1. **Scalability**:
   - Support an arbitrary number of secondary sink pads.
   - Allow for future extensions to support new metadata types or sources.

2. **Robustness**:
   - Handle missing buffers or metadata without pipeline disruption.
   - Ensure proper cleanup and resource management to prevent memory leaks.

3. **Maintainability**:
   - Use clear and modular design for ease of debugging and future enhancements.

4. **Compatibility**:
   - Ensure metadata compatibility with NVIDIA DeepStream for downstream processing.

5. **Testability**:
   - Design for comprehensive unit testing of metadata merging, synchronization, and caps negotiation.

---

### **5. Example Use Cases**

#### **Use Case 1: Primary Sink Pad Only**
- **Scenario**:
  - Only the primary sink pad is linked (no secondary pads with metadata).
- **Aggregator Behavior**:
  - The entire buffer from the primary sink pad is forwarded downstream without additional processing.

#### **Use Case 2: DeepStream and Optional NVIDIA Metadata**
- **Scenario**:
  - The primary input provides a video stream for analytics.
  - Multiple secondary sink pads carry metadata in **NVIDIA metadata format** (e.g., DeepStream, VMD).
  - Some secondary pads may not always be linked.
- **Aggregator Behavior**:
  - Synchronize metadata from all linked secondary pads with the correct primary video frames.
  - Merge metadata from secondary pads into the primary video frame.
  - Log appropriate warnings or info messages for missing buffers or metadata.
  - Preserve metadata compatibility with DeepStream downstream elements for further processing or visualization.

---

### **6. Derived Requirements**

1. **Input Handling**:
   - Support at least one primary sink pad and multiple optional secondary sink pads.
   - All pads must use **nvmm** format.

2. **Synchronization**:
   - Use default `next_time` behavior for timestamp alignment.

3. **Metadata Processing**:
   - Merge metadata from all secondary inputs into the primary frame.

4. **Output Handling**:
   - Forward the primary video stream with merged metadata.

5. **Performance**:
   - Real-time processing with minimal latency.

6. **Error Handling**:
   - Log info for missing buffers and warnings for buffers without metadata.

7. **Scalability**:
   - Support an arbitrary number of secondary pads.

---

### **7. Design Decisions (Class Structure)**

#### **Class: MyAggregator**
- Inherits: `GstAggregator`

#### **Attributes**:
1. **Primary Sink Pad**:
   - Type: `GstAggregatorPad`
   - Description: Handles the primary video stream with optional metadata.

2. **Secondary Sink Pads**:
   - Type: `GList` of `GstAggregatorPad`
   - Description: Handles metadata streams linked through multiple optional secondary pads.

3. **Output Pad**:
   - Type: `GstPad`
   - Description: Outputs the video stream with combined metadata.

#### **Methods**:
1. **aggregate()**:
   - Description: Core logic for:
     - Synchronizing buffers from all sink pads.
     - Extracting metadata from secondary inputs.
     - Merging metadata into the primary frame.
     - Logging missing buffers or metadata.
     - Pushing the aggregated buffer to the output pad.

2. **sink_event()**:
   - Description: Handles events on sink pads (e.g., CAPS, EOS, STREAM_START).

3. **get_caps()**:
   - Description: Negotiates caps based on the primary sink pad and downstream elements.

4. **pad_added()**:
   - Description: Dynamically adds new secondary sink pads.

5. **eos_handling()**:
   - Description: Manages EOS events across multiple sink pads, ensuring the pipeline stops gracefully.

#### **Custom Logic**:
- **Metadata Merging**:
  - Merges metadata (`NvDsBatchMeta`) from all available inputs.
  - Ensures compatibility with NVIDIA DeepStream.
- **Logging**:
  - Logs detailed info or warnings for missing buffers or metadata.

---

### **Next Steps**
1. Implement the outlined class structure.
2. Test each method with simulated input data.
3. Optimize for real-time performance.

---

Let me know if further refinements or additions are needed!


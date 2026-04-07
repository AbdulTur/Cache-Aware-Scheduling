# Report Outline

## Title Page

- Project title
- Student names
- Course code
- Date
- Abstract (150 to 200 words)

## 1. Introduction

- Real-time predictability problem
- Why caches complicate WCET estimation
- Objectives and research questions
- Learning outcomes addressed: LO1, LO2, LO3, LO6

## 2. Background

- RMS and EDF scheduling
- Cache-related preemption delay
- Cache partitioning and page coloring
- Related RTOS and real-time systems literature

## 3. System Design

- Discrete-event simulator architecture
- Task model
- Cache model
- Scheduling model
- Shared vs partitioned vs colored policy design
- Analytic CRPD bound approximation

## 4. Implementation

- C simulator structure
- CMake build and portability choices
- Python automation and plotting workflow
- Trace and CSV export pipeline

## 5. Evaluation

- Experiment scenarios
- Results table for each scheduler and cache policy
- CRPD comparison graph
- Deadline miss comparison graph
- Cross-task eviction graph
- Max response time graph
- Discussion of worst-case interference pattern
- Separate scheduler-comparison discussion for the non-overloaded scenario
- Compare base access utilization against isolated cold-WCET utilization
- Pairwise CRPD attribution heatmap
- Miss-penalty sensitivity study
- Timeline figure for one representative run

## 6. Discussion

- What improved predictability
- Tradeoff between isolation and effective cache capacity
- Limits of the simulator versus real hardware
- Lessons learned

## 7. Conclusion

- Summary of findings
- Recommendations
- Future work: better task traces, multi-core extension, formal CRPD analysis

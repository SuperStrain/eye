# Rockit DUMP Debug Skill Design

## Goal

Create a project-local OpenCode skill that diagnoses Rockit multimedia problems on Rockchip RV1126B devices. The skill uses the `DUMP调试信息说明` chapter of the following document as its authoritative DUMP reference:

`/home/yangy/atk_dlrv1126b_linux6.1_sdk/external/rockit/mpi/doc/Rockchip_Developer_Guide_MPI_CN.pdf`

The skill may correlate that reference with the current project's RV1126B source code, logs, and authorized read-only device inspection. It must not invent commands, node paths, fields, hardware parameters, or device connection details.

## Location And Files

Create the skill under the current project so its behavior remains scoped to this repository:

- `.opencode/skills/rockit-dump-debug/SKILL.md`
- `.opencode/skills/rockit-dump-debug/references/dump-debug.md`
- `.opencode/skills/rockit-dump-debug/references/eye-rv1126b.md`

`SKILL.md` defines triggering, the diagnostic workflow, evidence standards, and safety boundaries. `dump-debug.md` contains the verified chapter-derived command and field reference. `eye-rv1126b.md` maps the reference to this project's verified Rockit pipeline, runtime paths, and logs.

No automatic connection script is included. The skill does not store device credentials or modify application code.

## Triggering And Scope

Trigger the skill when a request or observed failure concerns RV1126B, Rockchip, Rockit, Rockit MPI, or the associated VI, VPSS, VENC, buffer, stream, frame-rate, timeout, no-image, corrupted-image, or pipeline behavior. Explicit requests for DUMP information also trigger it.

Before applying the workflow, confirm that the affected implementation is the RV1126B Rockit path. Compile and toolchain errors may be classified by the skill, but runtime DUMP collection is used only when the failure concerns the Rockit multimedia pipeline. Hi3516CV610 and unrelated multimedia implementations are outside scope.

## Diagnostic Architecture

The skill separates diagnosis into three layers:

1. The workflow layer collects symptoms, identifies the affected pipeline stage, and selects the minimum useful evidence.
2. The DUMP reference layer defines verified commands, nodes, fields, values, and cautions from the selected PDF chapter.
3. The project mapping layer connects that evidence to the current `eye` pipeline: `VI -> VENC -> StreamFetcher -> RTSP/file output`. VPSS remains part of the general Rockit DUMP reference, but the current RV1126B implementation does not create a VPSS group or channel.

This separation keeps the skill entry concise, avoids loading all reference material for every request, and allows the source-derived material and project-specific mapping to be reviewed independently.

## Diagnostic Workflow

The skill follows this sequence:

1. Confirm the target platform and Rockit involvement.
2. Collect the symptom, reproduction steps, occurrence time, relevant logs, module, device, group, and channel identifiers that are already known.
3. Map the symptom to the smallest likely pipeline stage: input, processing, encoding, or downstream delivery.
4. Select only the DUMP nodes and fields needed to test the current hypotheses. Do not request an indiscriminate full dump by default.
5. Prefer existing logs and user-provided command output before requesting a device connection.
6. If any actual connection attempt is considered necessary, whether requested by the user or initiated by the agent, complete the fixed five-item first-round table and a subsequent full confirmation round before any connection skill or connection/network tool is called.
7. Organize the result as observed facts, detected anomalies, inferences, and unresolved checks.
8. Recommend the smallest next verification action, including its expected result and risk.

The output contract has two evidence modes:

- For a single or single-source DUMP, use exactly five neutral sections: problem classification; verbatim raw evidence; the field four-column table; the minimum next command and expected result; risk. Do not emit independent candidate-cause, most-likely-cause, or causal-confidence sections. The fixed statement `现有证据仅支持上述优先检查顺序，不支持确定因果。` follows the field table.
- Only after at least two mutually independent evidence items cross-validate may the output add candidate causes, a most-likely cause, and confidence. Each item must cite its supporting evidence, while retaining unverified alternatives, raw evidence, field interpretation, next verification, expected result, and safety cautions.

Correlation alone is not presented as root cause. When evidence is insufficient, the skill reports uncertainty and asks for the specific missing information.

## Device Access And Safety

Remote access defaults to a two-round gate before any actual connection attempt. The skill must confirm:

- connection protocol, such as Telnet, SSH, or serial;
- device address or serial endpoint;
- account and a safe credential-provisioning method;
- authorization to connect;
- permitted command scope.

The first round may only show the fixed five-row table and request missing values. A later round must explicitly confirm all parsed values before `embedded-device-debug` or any connection/network tool is called. Previous connections, persistent configuration, agent initiative, or an initial “connect now” request are not confirmation.

The initial device session is read-only. It may inspect system identity, running processes, logs, and verified Rockit DUMP information. It must not guess connection values or silently reuse credentials.

Restarting services, killing processes, changing configuration, changing stream parameters, rebuilding a pipeline, clearing state, or writing files requires separate explicit approval. If connection fails, a command is absent, permissions are insufficient, or device output differs from the document, the skill reports the failure and provides a manual collection path instead of fabricating results.

## Knowledge Extraction

Extract only the complete `DUMP调试信息说明` chapter boundary needed by this skill. Preserve the relationships among commands, modules, nodes, fields, values, and cautions, but rewrite the content into a concise diagnostic reference rather than copying long passages.

PDF extraction must account for scanned pages, broken line wrapping, and table-layout errors. Commands and field tables are checked against the rendered source pages. Unreadable or ambiguous text is marked as unverified and is not reconstructed by assumption.

The project mapping reference records only facts confirmed by current source code and repository instructions, including pipeline topology, channel relationships, runtime paths, and logs relevant to RV1126B diagnosis.

If the PDF, project source, and device output conflict, preserve all three pieces of evidence. Runtime output and actual code behavior guide the immediate diagnosis, while the mismatch is reported as a likely document, SDK, or firmware version difference.

## Error Handling

The skill uses these fallback rules:

- Missing symptom details: ask for reproduction steps, time range, logs, and affected stream.
- Unknown module or channel: derive it from project code when possible; otherwise ask rather than assume.
- Missing DUMP command or node: report a version difference and gather available module, process, and log evidence.
- Permission failure: report the exact denial and suggest the minimum privilege or user-run command needed.
- Device unavailable: provide commands for manual execution and explain what output to return.
- Ambiguous PDF extraction: mark the item unverified and omit it from executable guidance.
- Conflicting evidence: show the conflict and propose a discriminating check.

## Verification

Validate the finished skill with representative prompts and static checks:

1. `RV1126B VENC 取流超时` triggers Rockit DUMP diagnosis and requests targeted evidence.
2. A supplied DUMP output receives field-level interpretation that separates facts from inferences.
3. A request to inspect a board causes the skill to ask for protocol, address, credential handling, authorization, and command scope before connecting.
4. Without approval, the skill does not restart processes, modify configuration, clear state, or change pipeline parameters.
5. A missing command or node is handled as a possible SDK or firmware difference, with an alternate collection method.
6. A Hi3516CV610 or non-Rockit problem does not enter the Rockit-specific DUMP workflow.
7. Every documented command, node, and field can be traced to the selected PDF chapter; every project-specific statement can be traced to repository code or instructions.

Also validate the skill metadata and directory structure using the skill-authoring tools prescribed by the active OpenCode environment.

## Non-Goals

- Automating credentials or unattended device login.
- Performing destructive or behavior-changing device operations without separate approval.
- Diagnosing HiSilicon MPP through Rockit-specific guidance.
- Treating the entire MPI guide as the skill knowledge base.
- Changing the `eye` multimedia implementation.
- Git versioning or collaboration distribution of this local project skill; the delivery target is the current workspace's local OpenCode skill.
- Changing the existing `.gitignore` rule that ignores `.opencode`. If Git distribution is later needed, the user must separately decide whether to add a `.gitignore` exception or use an explicit add strategy.

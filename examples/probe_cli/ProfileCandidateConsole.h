// SPDX-License-Identifier: MIT
// Included in ProbeConsole.cpp inside the console namespace.
bool Console::formatProfileStage(uint32_t id, uint32_t commandId, const ProfileCandidateResult& r, bool inspection) noexcept {
    const auto& v = r.stage;
    std::size_t used = 0;
    if (!append(output_, sizeof(output_), used,
        "{\"type\":\"%s\",\"profile\":\"ess_rs\",\"id\":%lu,\"command\":\"%s\",\"command_id\":%lu,\"operation_id\":%lu,"
        "\"profile_stage\":true,\"recovery\":false,\"pending\":%s,\"ok\":%s,\"result\":\"%s\",\"address\":%u,"
        "\"target_generation\":%lu,\"configuration_generation\":%lu,\"serial_generation\":%lu,\"word_order\":%u,"
        "\"staged\":%s,\"unchanged\":%s,\"cancelled\":%s,\"execution\":\"%s\",\"error\":\"%s\","
        "\"started_us\":%llu,\"deadline_us\":%llu,\"finished_us\":%llu,\"readback_known\":%s",
        inspection ? "reply" : "profile_stage", static_cast<unsigned long>(id), inspection ? "result" : "motion-profile-stage",
        static_cast<unsigned long>(commandId), static_cast<unsigned long>(v.operationId), boolean(v.pending),
        boolean(v.pending || v.staged), v.pending ? "pending" : "done", v.target.address,
        static_cast<unsigned long>(v.target.generation), static_cast<unsigned long>(v.configurationGeneration),
        static_cast<unsigned long>(v.serialGeneration), static_cast<unsigned>(r.candidate.wordOrder),
        boolean(v.staged), boolean(r.unchanged), boolean(v.cancelled), executionName(v.execution), r.error,
        static_cast<unsigned long long>(v.startedUs), static_cast<unsigned long long>(v.deadlineUs),
        static_cast<unsigned long long>(v.finishedUs), boolean(v.readbackKnown))) return false;
    const uint16_t* arrays[] = {r.candidate.words, r.original, r.before, v.readback};
    const char* names[] = {"candidate", "original", "before", "readback"};
    const unsigned sizes[] = {5, 6, 6, 5};
    for (unsigned i = 0; i < 4; ++i) {
        if (!append(output_, sizeof(output_), used, ",\"%s\":[", names[i])) return false;
        for (unsigned n = 0; n < sizes[i]; ++n)
            if (!append(output_, sizeof(output_), used, "%s%u", n ? "," : "", arrays[i][n])) return false;
        if (!append(output_, sizeof(output_), used, "]")) return false;
    }
    if (!append(output_, sizeof(output_), used, ",\"prior_evidence\":") ||
        !actionEvidence(output_, sizeof(output_), used, r.priorEvidence) ||
        !append(output_, sizeof(output_), used, ",\"write_evidence\":") ||
        !actionEvidence(output_, sizeof(output_), used, v.writeEvidence) ||
        !append(output_, sizeof(output_), used, ",\"readback_evidence\":") ||
        !actionEvidence(output_, sizeof(output_), used, v.readEvidence) ||
        !append(output_, sizeof(output_), used, "}") || !appendReportSerial(used)) return false;
    emit(inspection ? 0 : v.operationId); return true;
}
bool Console::reportProfileStage(uint32_t id, uint32_t operationId, const ProfileCandidateResult& result,
                                const HostTuple* tuple, uint32_t serialGeneration) noexcept {
    setReportSerial(tuple, serialGeneration);
    if (outputPending() || result.stage.pending || result.stage.operationId != operationId) return false;
    for (auto& item : outstanding_) if (item.commandId == id && item.operationId == operationId && !item.transferred) {
        outputFormat_ = item.format; syntax_ = helpCommand_ = nullptr;
        if (!formatProfileStage(id, id, result, false)) return false;
        if (item.operationId == operationId) item.transferred = true;
        return true;
    }
    return false;
}

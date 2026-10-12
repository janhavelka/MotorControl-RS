// SPDX-License-Identifier: MIT
#include "HomeConsoleFormat.h"
#include "ConsoleReport.h"

namespace MotorControlRSExample { namespace Probe { namespace HomeConsole {
namespace Core = MotorControlRS;
namespace Ess = MotorControlRS::ESS_RS;
using ConsoleReport::append;
using ConsoleReport::boolean;
using ConsoleReport::hex;
using ConsoleReport::executionName;
using ConsoleReport::motorOutcome;
namespace {
// Homing retains eight observations. One fixed column schema keeps every
// provenance field within the existing line bound, including 64-bit extremes.
bool homeEvidence(char* output, std::size_t capacity, std::size_t& used, const Ess::ActionEvidence& e) {
    char raw[Ess::ACTION_MAX_REPLY_BYTES * 2 + 1]; hex(e.raw, e.length, raw, sizeof(raw));
    return append(output, capacity, used,
        "[%u,%u,\"%s\",%u,%u,%s,%s,%s,%s,%llu,%llu,%llu,%ld,\"%s\",%ld,%u]",
        e.step, static_cast<unsigned>(e.event), raw, static_cast<unsigned>(e.receivedLength), static_cast<unsigned>(e.txAccepted),
        boolean(e.txComplete), boolean(e.responseConfirmed), boolean(e.qualified), boolean(e.executionUnknown),
        static_cast<unsigned long long>(e.earliestUs), static_cast<unsigned long long>(e.latestUs), static_cast<unsigned long long>(e.deliveredUs),
        static_cast<long>(e.transportDetail), Core::errToString(e.status.code), static_cast<long>(e.status.detail), static_cast<unsigned>(e.frameError));
}
}
bool formatResult(char* output, std::size_t capacity, std::size_t& used,
                  uint32_t id, uint32_t commandId, uint32_t operationId,
                  const Ess::HomeContext& c, bool inspection, bool interruptedByStop) noexcept {
    used = 0;
    const bool ok = c.state == Core::ActionState::SUCCEEDED && c.completion == Core::ActionCompletion::OBSERVED;
    return append(output, capacity, used,
        "{\"type\":\"%s\",\"profile\":\"ess_rs\",\"id\":%lu,\"command\":\"%s\",\"command_id\":%lu,\"operation_id\":%lu,\"home\":true,\"read_kind\":null,\"action_kind\":null,\"move_kind\":null,\"capture_read\":false,\"recovery\":false,\"ok\":%s,\"state\":\"%s\",\"outcome\":\"%s\",\"status\":\"%s\",\"detail\":%ld,\"setup_execution\":\"%s\",\"execution\":\"%s\",\"completion\":\"%s\",\"target\":%lu,\"address\":%u,\"generation\":%lu,\"configuration_generation\":%lu,\"started_us\":%llu,\"deadline_us\":%llu,\"serviced_us\":%llu,\"polls\":%u,\"phase\":%u,\"method\":%d,\"staging_words\":[%u,%u,%u,%u,%u,%u],\"staging_applied\":%s,\"uncertain\":%s,\"running_observed\":%s,\"homed_low_observed\":%s,\"observation_known\":%s,\"raw_alarm\":%u,\"raw_motion\":%u,\"raw_position_words\":[%u,%u],\"interrupted_by_stop\":%s,\"prerequisites\":{\"observed_us\":%llu,\"maximum_age_us\":%llu,\"raw_motion\":%u,\"auxiliary\":%u,\"reference_semantics_qualified\":%s,\"qualified_parameters\":[%d,%u,%u,%u]},\"completion_observed_us\":%llu,\"evidence_columns\":[\"step\",\"event\",\"raw_hex\",\"received_length\",\"tx_accepted\",\"tx_complete\",\"response_confirmed\",\"qualified\",\"execution_unknown\",\"earliest_us\",\"latest_us\",\"delivered_us\",\"transport_detail\",\"status\",\"detail\",\"frame_error\"],\"staging_evidence\":",
        inspection ? "reply" : "home", static_cast<unsigned long>(id), inspection ? "result" : "home",
        static_cast<unsigned long>(commandId), static_cast<unsigned long>(operationId), boolean(ok),
        c.state == Core::ActionState::SUCCEEDED ? "succeeded" : "failed", motorOutcome(c.outcome), Core::errToString(c.status.code), static_cast<long>(c.status.detail),
        executionName(c.setupExecution), executionName(c.execution), c.completion == Core::ActionCompletion::OBSERVED ? "observed" : "not_observed",
        static_cast<unsigned long>(c.target.id), c.target.address, static_cast<unsigned long>(c.target.generation), static_cast<unsigned long>(c.request.configurationGeneration),
        static_cast<unsigned long long>(c.startedUs), static_cast<unsigned long long>(c.deadlineUs), static_cast<unsigned long long>(c.servicedUs), c.polls, static_cast<unsigned>(c.phase), static_cast<int>(c.request.method),
        c.words[0], c.words[1], c.words[2], c.words[3], c.words[4], c.words[5], boolean(c.stagingApplied), boolean(c.uncertain), boolean(c.runningObserved), boolean(c.homedLowObserved), boolean(c.observationKnown),
        c.rawAlarm, c.rawMotion, c.rawPositionWords[0], c.rawPositionWords[1], boolean(interruptedByStop),
        static_cast<unsigned long long>(c.prerequisites.observedUs), static_cast<unsigned long long>(c.prerequisites.maximumAgeUs), c.prerequisites.rawMotion, static_cast<unsigned>(c.prerequisites.auxiliary), boolean(c.prerequisites.referenceSemanticsQualified), static_cast<int>(c.prerequisites.qualifiedMethod),
        c.prerequisites.qualifiedSearchSpeed, c.prerequisites.qualifiedReturnSpeed, c.prerequisites.qualifiedRampTime, static_cast<unsigned long long>(c.completionObservedUs)) &&
        homeEvidence(output, capacity, used, c.stagingEvidence) && append(output, capacity, used, ",\"trigger_evidence\":") &&
        homeEvidence(output, capacity, used, c.triggerEvidence) && append(output, capacity, used, ",\"activity_evidence\":") &&
        homeEvidence(output, capacity, used, c.activityEvidence) && append(output, capacity, used, ",\"low_evidence\":") &&
        homeEvidence(output, capacity, used, c.lowEvidence) && append(output, capacity, used, ",\"last_observation\":") &&
        homeEvidence(output, capacity, used, c.lastObservation) && append(output, capacity, used, ",\"completion_evidence\":") &&
        homeEvidence(output, capacity, used, c.completionEvidence) && append(output, capacity, used, ",\"zero_evidence\":") &&
        homeEvidence(output, capacity, used, c.zeroEvidence) && append(output, capacity, used, ",\"failure_evidence\":") &&
        homeEvidence(output, capacity, used, c.failureEvidence) && append(output, capacity, used, "}");
}
bool formatMethods(char* output, std::size_t capacity, std::size_t& used, uint32_t id) noexcept {
    used = 0;
    bool fits = append(output, capacity, used,
        "{\"type\":\"reply\",\"profile\":\"ess_rs\",\"id\":%lu,\"command\":\"home\",\"ok\":true,\"methods\":[", static_cast<unsigned long>(id));
    for (uint8_t i = 0; fits && i < Ess::HOME_METHOD_COUNT; ++i) {
        const auto* method = Ess::homeMethodAt(i);
        fits = method && append(output, capacity, used,
            "%s{\"method\":%d,\"support\":\"%s\",\"inputs\":%u,\"index\":%s,\"moves\":%s,\"reason\":%u}",
            i ? "," : "", static_cast<int>(method->method),
            method->support == Ess::HomeSupport::IMPLEMENTED ? "implemented" : method->support == Ess::HomeSupport::UNIMPLEMENTED ? "unimplemented" : "unresolved",
            method->requiredInputs, boolean(method->requiresIndex), boolean(method->moves),
            method->support == Ess::HomeSupport::UNIMPLEMENTED ? 0U : static_cast<int>(method->method) < 0 ? 1U :
            static_cast<int>(method->method) == 18 ? 2U : static_cast<unsigned>(method->method) - 30U);
    }
    fits = fits && append(output, capacity, used, "],\"reasons\":{\"0\":\"switch_edges_and_return_pending\",\"1\":\"collision_encoding_and_parameter_conflicts\",\"2\":\"limit_prose_diagram_conflict\",\"3\":\"negative_internal_index_search\",\"4\":\"positive_internal_index_search\",\"5\":\"current_position_origin\"},\"physical_qualified\":false,\"offset\":\"zero_only\",\"auxiliary\":7}");
    return fits;
}
}}}

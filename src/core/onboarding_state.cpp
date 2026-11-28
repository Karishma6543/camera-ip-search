#include "onboarding_state.h"

OnboardingState::Result OnboardingState::evaluate(const OnboardingEvidence &evidence)
{
    if (!evidence.observedOnNetwork)
        return {State::NotObserved, QStringLiteral("Not observed"),
                QStringLiteral("Connect the camera and select the correct network interface.")};

    if (!evidence.networkReachable)
        return {State::Candidate, QStringLiteral("Candidate"),
                QStringLiteral("Check camera power, Ethernet link, subnet and firewall; then retry discovery.")};

    if (!evidence.identityVerified)
        return {State::Reachable, QStringLiteral("Reachable — identity unverified"),
                QStringLiteral("Run a supported identity probe; do not assume this endpoint is a camera.")};

    if (evidence.operationFailed)
        return {State::Failed, QStringLiteral("Operation failed"),
                QStringLiteral("Review the sanitized diagnostic and use the documented recovery procedure before retrying.")};

    if (evidence.activationInProgress)
        return {State::InitializationInProgress, QStringLiteral("Initialization in progress"),
                QStringLiteral("Wait for the documented activation operation to finish or cancel it safely.")};

    if (evidence.authenticationRequired)
        return {State::AuthenticationRequired, QStringLiteral("Authentication required"),
                QStringLiteral("Enter credentials for this device; credentials are not inferred or guessed.")};

    if (evidence.activationStatusKnown && evidence.activationRequired) {
        if (evidence.supportedAdapterAvailable)
            return {State::InitializationRequired, QStringLiteral("Initialization required"),
                    QStringLiteral("Review the documented activation method and confirm before proceeding.")};
        return {State::Unsupported, QStringLiteral("Initialization unsupported"),
                QStringLiteral("No authorized activation adapter is available for this model and firmware.")};
    }

    if (evidence.activationStatusKnown && !evidence.activationRequired &&
        evidence.activationVerified && !evidence.networkConfigurationVerified) {
        if (evidence.configurationInProgress)
            return {State::Configuring, QStringLiteral("Configuring network"),
                    QStringLiteral("Wait for configuration read-back and reconnection verification.")};
        if (evidence.configurationRequired)
            return {State::ConfigurationRequired, QStringLiteral("Network configuration required"),
                    QStringLiteral("Review the proposed network settings and confirm before applying them.")};
        return {State::Initialized, QStringLiteral("Initialized — configuration not verified"),
                QStringLiteral("Read back the camera network settings before marking it ready.")};
    }

    if (!evidence.supportedAdapterAvailable)
        return {State::Unsupported, QStringLiteral("Device identified — configuration unsupported"),
                QStringLiteral("Discovery succeeded, but no compatible configuration adapter is available.")};

    if (!evidence.activationStatusKnown || !evidence.activationVerified)
        return {State::VerificationPending, QStringLiteral("Activation verification pending"),
                QStringLiteral("Confirm activation state through an authoritative adapter response before marking ready.")};

    if (evidence.activationRequired)
        return {State::InitializationRequired, QStringLiteral("Initialization required"),
                QStringLiteral("Complete activation through an authorized adapter.")};

    if (evidence.configurationInProgress)
        return {State::Configuring, QStringLiteral("Configuring network"),
                QStringLiteral("Wait for configuration read-back and reconnection verification.")};

    if (evidence.configurationRequired)
        return {State::ConfigurationRequired, QStringLiteral("Network configuration required"),
                QStringLiteral("Review the proposed network settings and confirm before applying them.")};

    if (!evidence.networkConfigurationVerified)
        return {State::VerificationPending, QStringLiteral("Network verification pending"),
                QStringLiteral("Read back the network configuration before marking the device ready.")};

    return {State::Ready, QStringLiteral("Ready"),
            QStringLiteral("Device identity, activation state and network configuration have been verified.")};
}

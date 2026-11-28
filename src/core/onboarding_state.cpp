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

    if (evidence.activationStatusKnown && evidence.activationRequired) {
        if (evidence.supportedAdapterAvailable)
            return {State::InitializationRequired, QStringLiteral("Initialization required"),
                    QStringLiteral("Review the documented activation method and confirm before proceeding.")};
        return {State::Unsupported, QStringLiteral("Initialization unsupported"),
                QStringLiteral("No authorized activation adapter is available for this model and firmware.")};
    }

    if (evidence.authenticationRequired)
        return {State::AuthenticationRequired, QStringLiteral("Authentication required"),
                QStringLiteral("Enter credentials for this device; credentials are not inferred or guessed.")};

    if (!evidence.supportedAdapterAvailable)
        return {State::Unsupported, QStringLiteral("Device identified — configuration unsupported"),
                QStringLiteral("Discovery succeeded, but no compatible configuration adapter is available.")};

    if (!evidence.networkConfigurationVerified)
        return {State::VerificationPending, QStringLiteral("Verification pending"),
                QStringLiteral("Read the network configuration and verify supported settings before marking ready.")};

    return {State::Ready, QStringLiteral("Ready"),
            QStringLiteral("Device identity and network configuration have been verified.")};
}

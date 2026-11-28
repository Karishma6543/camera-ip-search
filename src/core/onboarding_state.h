#pragma once

#include <QString>

struct OnboardingEvidence
{
    bool observedOnNetwork{false};
    bool networkReachable{false};
    bool identityVerified{false};
    bool activationStatusKnown{false};
    bool activationRequired{false};
    bool activationInProgress{false};
    bool activationVerified{false};
    bool authenticationRequired{false};
    bool supportedAdapterAvailable{false};
    bool configurationRequired{false};
    bool configurationInProgress{false};
    bool networkConfigurationVerified{false};
    bool operationFailed{false};
};

class OnboardingState final
{
public:
    enum class State {
        NotObserved,
        Candidate,
        Reachable,
        Identified,
        InitializationRequired,
        InitializationInProgress,
        Initialized,
        AuthenticationRequired,
        ConfigurationRequired,
        Configuring,
        Unsupported,
        VerificationPending,
        Ready,
        Failed
    };

    struct Result {
        State state{State::NotObserved};
        QString label;
        QString nextAction;
    };

    // Activation is reported as required only when explicit protocol evidence
    // says so; vendor names, open ports and missing credentials are not evidence.
    // Ready requires verified identity, activation state and network configuration.
    static Result evaluate(const OnboardingEvidence &evidence);
};

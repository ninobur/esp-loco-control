#include "NaviIntegratedCore.h"
using namespace navi_eyes;
const char* nm(EwoEventKind k){ switch(k){
    case EwoEventKind::Declared: return "DECLARED";
    case EwoEventKind::Reversed: return "REVERSED";
    case EwoEventKind::HallSupport: return "HALL_SUPPORT";
    case EwoEventKind::TargetConfirmed: return "TARGET_CONFIRMED";
    case EwoEventKind::MissedMagnet: return "MISSED_MAGNET";
    case EwoEventKind::ReferenceReady: return "INITIAL_REFERENCE";
    case EwoEventKind::SpatialClearance: return "SPATIAL_CLEARANCE";
    case EwoEventKind::SpatialCollect: return "SPATIAL_COLLECT";
    case EwoEventKind::SpatialReady: return "SPATIAL_REFERENCE";
    case EwoEventKind::SpatialEmpty: return "SPATIAL_EMPTY_RETAINED_REFERENCE";
    case EwoEventKind::IrDistanceHold: return "IR_DISTANCE_HOLD";
    case EwoEventKind::IrDistanceReady: return "IR_DISTANCE_READY";
    case EwoEventKind::PwmZeroDisplacement: return "PWM_ZERO_IR_DISPLACEMENT";
    case EwoEventKind::ObservationLoss: return "OBSERVATION_LOSS";
    case EwoEventKind::SpatialInvalidated: return "SPATIAL_INVALIDATED";
    case EwoEventKind::HallEncounter: return "HALL_ENCOUNTER";
    case EwoEventKind::DnaPending: return "DNA_PENDING";
    case EwoEventKind::DnaResolved: return "DNA_RESOLVED";
    case EwoEventKind::DnaUnresolvedStop: return "DNA_UNRESOLVED_STOP";
    case EwoEventKind::SpatialTimeout: return "SPATIAL_TIMEOUT_RETAINED_REFERENCE";
    default: return "NONE";
}} int main(){return nm(EwoEventKind::None)[0];}

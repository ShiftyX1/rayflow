#pragma once

// Engine UI View Model
// This file provides the base view model types for the engine UI system.
// Games should extend these types in their own view model files.

#include "ui_view_model_base.hpp"

// For backwards compatibility during migration, re-export all types
// TODO: Games should migrate to use ui_view_model_base.hpp directly
// and define their own game-specific extensions

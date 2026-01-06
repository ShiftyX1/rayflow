package cmd

import (
	"fmt"
	"os"
	"path/filepath"
	"strings"

	"github.com/spf13/cobra"
)

var initCmd = &cobra.Command{
	Use:   "init <game_name>",
	Short: "Initialize a new Rayflow game project",
	Long: `Creates a new game project structure.

By default, creates in games/ directory (for engine development).
Use --standalone to create a standalone project that uses Rayflow SDK.

Example:
  rayflow-cli init my_game                           # Inside engine repo
  rayflow-cli init my_game --standalone              # Standalone project
  rayflow-cli init survival --display-name "Survival Game"

Standalone project will be created in current directory:
  my_game/
  ├── game.toml           # Project manifest
  ├── CMakeLists.txt      # Build configuration (uses find_package)
  ├── src/
  │   ├── shared/         # Protocol, shared types
  │   ├── server/         # Server-side logic
  │   ├── client/         # Client-side rendering
  │   └── app/            # Entry points
  └── resources/          # Game assets`,
	Args: cobra.ExactArgs(1),
	RunE: runInit,
}

var (
	displayName string
	author      string
	standalone  bool
	sdkVersion  string
)

func init() {
	initCmd.Flags().StringVar(&displayName, "display-name", "", "Display name for the game (default: game_name)")
	initCmd.Flags().StringVar(&author, "author", "", "Author name")
	initCmd.Flags().BoolVar(&standalone, "standalone", false, "Create standalone project (uses SDK via find_package)")
	initCmd.Flags().StringVar(&sdkVersion, "sdk-version", "latest", "SDK version to use (standalone only)")
}

func runInit(cmd *cobra.Command, args []string) error {
	gameName := args[0]

	if !isValidGameName(gameName) {
		return fmt.Errorf("invalid game name '%s': use lowercase letters, numbers, and underscores only", gameName)
	}

	if displayName == "" {
		displayName = strings.ReplaceAll(strings.Title(strings.ReplaceAll(gameName, "_", " ")), " ", "")
	}

	var gameDir string
	var sdkDir string

	if standalone {
		cwd, err := os.Getwd()
		if err != nil {
			return fmt.Errorf("could not get current directory: %w", err)
		}
		gameDir = filepath.Join(cwd, gameName)

		actualVersion := sdkVersion
		if sdkVersion == "latest" {
			fmt.Println("Fetching latest SDK version...")
			latest, err := GetLatestSDKVersion()
			if err != nil {
				return fmt.Errorf("failed to get latest version: %w", err)
			}
			actualVersion = latest.Version
			fmt.Printf("Latest version: %s\n\n", actualVersion)
		}

		sdkDir = GetSDKDir(actualVersion)
		if !IsSDKInstalled(actualVersion) {
			fmt.Printf("SDK version %s not found. Installing...\n\n", actualVersion)
			if err := installSDK(actualVersion); err != nil {
				return fmt.Errorf("failed to install SDK: %w", err)
			}
			fmt.Println()
		} else {
			fmt.Printf("Using SDK version %s from %s\n\n", actualVersion, sdkDir)
		}
	} else {
		engineRoot, err := findEngineRoot()
		if err != nil {
			return fmt.Errorf("could not find Rayflow engine root (use --standalone for external projects): %w", err)
		}
		gameDir = filepath.Join(engineRoot, "games", gameName)
	}

	if _, err := os.Stat(gameDir); err == nil {
		return fmt.Errorf("game '%s' already exists at %s", gameName, gameDir)
	}

	fmt.Printf("Creating game '%s' in %s...\n\n", gameName, gameDir)

	dirs := []string{
		"src/shared",
		"src/server",
		"src/client",
		"src/app",
		"resources/textures",
		"resources/maps",
		"resources/ui",
	}

	for _, dir := range dirs {
		path := filepath.Join(gameDir, dir)
		if err := os.MkdirAll(path, 0755); err != nil {
			return fmt.Errorf("failed to create directory %s: %w", path, err)
		}
		fmt.Printf("  %s/\n", dir)
	}

	var cmakeContent string
	if standalone {
		cmakeContent = generateCMakeListsStandalone(gameName, displayName, sdkDir)
	} else {
		cmakeContent = generateCMakeLists(gameName, displayName)
	}

	files := map[string]string{
		"game.toml":                  generateGameToml(gameName, displayName, author),
		"CMakeLists.txt":             cmakeContent,
		"src/shared/protocol.hpp":    generateProtocolHpp(gameName, standalone),
		"src/shared/protocol.cpp":    generateProtocolCpp(gameName, standalone),
		"src/server/game_server.hpp": generateServerHpp(gameName, displayName, standalone),
		"src/server/game_server.cpp": generateServerCpp(gameName, displayName, standalone),
		"src/client/game_client.hpp": generateClientHpp(gameName, displayName, standalone),
		"src/client/game_client.cpp": generateClientCpp(gameName, displayName, standalone),
		"src/app/client_main.cpp":    generateClientMain(gameName, displayName, standalone),
		"src/app/server_main.cpp":    generateServerMain(gameName, displayName, standalone),
		"README.md":                  generateReadme(gameName, displayName),
	}

	for filename, content := range files {
		path := filepath.Join(gameDir, filename)
		if err := os.WriteFile(path, []byte(content), 0644); err != nil {
			return fmt.Errorf("failed to write %s: %w", path, err)
		}
		fmt.Printf("  %s\n", filename)
	}

	fmt.Printf("\nGame '%s' created successfully!\n\n", gameName)
	fmt.Println("Next steps:")
	fmt.Printf("  1. cd %s\n", gameDir)

	if standalone {
		fmt.Println("  2. cmake -B build")
		fmt.Println("  3. cmake --build build")
		fmt.Printf("  4. ./build/%s\n", gameName)
	} else {
		fmt.Println("  2. rayflow-cli build")
		fmt.Println("  3. rayflow-cli run")
		fmt.Println()
		fmt.Println("Or build from engine root:")
		fmt.Println("  cmake --build --preset debug")
	}

	return nil
}

func isValidGameName(name string) bool {
	if len(name) == 0 {
		return false
	}
	for _, c := range name {
		if !((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_') {
			return false
		}
	}
	return name[0] >= 'a' && name[0] <= 'z'
}

func findEngineRoot() (string, error) {
	dir, err := os.Getwd()
	if err != nil {
		return "", err
	}

	for {
		engineDir := filepath.Join(dir, "engine")
		gamesDir := filepath.Join(dir, "games")

		if isDir(engineDir) && isDir(gamesDir) {
			return dir, nil
		}

		if filepath.Base(filepath.Dir(dir)) == "games" {
			return filepath.Dir(filepath.Dir(dir)), nil
		}

		parent := filepath.Dir(dir)
		if parent == dir {
			break
		}
		dir = parent
	}

	return "", fmt.Errorf("not inside a Rayflow engine directory")
}

func isDir(path string) bool {
	info, err := os.Stat(path)
	return err == nil && info.IsDir()
}

func GetInitCmd() *cobra.Command {
	return initCmd
}

func installSDK(version string) error {
	return runSDKInstall(nil, []string{version})
}

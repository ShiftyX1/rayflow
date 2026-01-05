package cmd

import (
	"fmt"
	"os"
	"os/exec"
	"path/filepath"

	"github.com/spf13/cobra"
)

var buildCmd = &cobra.Command{
	Use:   "build",
	Short: "Build the game project",
	Long: `Build the current game or all games using CMake.

Examples:
  rayflow-cli build              # Build all (debug)
  rayflow-cli build --release    # Build all (release)
  rayflow-cli build my_game      # Build specific game`,
	RunE: runBuild,
}

var (
	buildRelease bool
	buildTarget  string
)

func init() {
	buildCmd.Flags().BoolVar(&buildRelease, "release", false, "Build in release mode")
	buildCmd.Flags().StringVarP(&buildTarget, "target", "t", "", "Specific target to build")
}

func runBuild(cmd *cobra.Command, args []string) error {
	engineRoot, err := findEngineRoot()
	if err != nil {
		return fmt.Errorf("not in a Rayflow project: %w", err)
	}
	
	preset := "debug"
	if buildRelease {
		preset = "release"
	}
	
	buildDir := filepath.Join(engineRoot, "build", preset)
	if _, err := os.Stat(filepath.Join(buildDir, "build.ninja")); os.IsNotExist(err) {
		fmt.Printf("Configuring %s build...\n", preset)
		configCmd := exec.Command("cmake", "--preset", preset)
		configCmd.Dir = engineRoot
		configCmd.Stdout = os.Stdout
		configCmd.Stderr = os.Stderr
		if err := configCmd.Run(); err != nil {
			return fmt.Errorf("cmake configure failed: %w", err)
		}
	}
	
	buildArgs := []string{"--build", "--preset", preset}
	
	if buildTarget != "" {
		buildArgs = append(buildArgs, "--target", buildTarget)
	} else if len(args) > 0 {
		gameName := args[0]
		buildArgs = append(buildArgs, "--target", gameName, "--target", gameName+"_server")
	}
	
	fmt.Printf("Building (%s)...\n", preset)
	buildExec := exec.Command("cmake", buildArgs...)
	buildExec.Dir = engineRoot
	buildExec.Stdout = os.Stdout
	buildExec.Stderr = os.Stderr
	
	if err := buildExec.Run(); err != nil {
		return fmt.Errorf("build failed: %w", err)
	}
	
	fmt.Println("\nBuild complete!")
	return nil
}

func GetBuildCmd() *cobra.Command {
	return buildCmd
}

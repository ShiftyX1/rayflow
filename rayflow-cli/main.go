package main

import (
	"os"

	"pulsestudio/rayflow-cli/cmd"

	"github.com/spf13/cobra"
)

var rootCmd = &cobra.Command{
	Use:   "rayflow-cli",
	Short: "Rayflow Engine CLI",
	Long: `Rayflow CLI - Command-line tools for Rayflow Engine game development.

Commands:
  init    Create a new game project
  build   Build games
  run     Run a game
  sdk     Manage SDK installations

Examples:
  rayflow-cli init my_game
  rayflow-cli init my_game --standalone
  rayflow-cli sdk install
  rayflow-cli build
  rayflow-cli run my_game`,
}

func init() {
	rootCmd.AddCommand(cmd.GetInitCmd())
	rootCmd.AddCommand(cmd.GetBuildCmd())
	rootCmd.AddCommand(cmd.GetRunCmd())
	rootCmd.AddCommand(cmd.GetSDKCmd())
}

func main() {
	if err := rootCmd.Execute(); err != nil {
		os.Exit(1)
	}
}

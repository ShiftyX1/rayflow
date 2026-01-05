package cmd

import (
	"fmt"
	"os"
	"os/exec"
	"path/filepath"

	"github.com/spf13/cobra"
)

var runCmd = &cobra.Command{
	Use:   "run <game_name>",
	Short: "Run a game",
	Long: `Run the client or server for a game.

Examples:
  rayflow-cli run my_game                    # Run client (singleplayer)
  rayflow-cli run my_game --server           # Run dedicated server
  rayflow-cli run my_game --connect host:port  # Connect to server`,
	Args: cobra.ExactArgs(1),
	RunE: runRun,
}

var (
	runServer    bool
	runConnect   string
	runPort      int
	runName      string
)

func init() {
	runCmd.Flags().BoolVar(&runServer, "server", false, "Run dedicated server instead of client")
	runCmd.Flags().StringVar(&runConnect, "connect", "", "Connect to server (host:port)")
	runCmd.Flags().IntVar(&runPort, "port", 7777, "Server port (for --server)")
	runCmd.Flags().StringVar(&runName, "name", "Player", "Player name")
}

func runRun(cmd *cobra.Command, args []string) error {
	gameName := args[0]
	
	engineRoot, err := findEngineRoot()
	if err != nil {
		return fmt.Errorf("not in a Rayflow project: %w", err)
	}
	
	buildDir := filepath.Join(engineRoot, "build", "debug")
	
	var exePath string
	var execArgs []string
	
	if runServer {
		exePath = filepath.Join(buildDir, gameName+"_server")
		execArgs = append(execArgs, "--port", fmt.Sprintf("%d", runPort))
	} else {
		exePath = filepath.Join(buildDir, gameName)
		if runConnect != "" {
			execArgs = append(execArgs, "--connect", runConnect)
		}
		execArgs = append(execArgs, "--name", runName)
	}
	
	if _, err := os.Stat(exePath); os.IsNotExist(err) {
		return fmt.Errorf("executable not found: %s\nRun 'rayflow-cli build %s' first", exePath, gameName)
	}
	
	fmt.Printf("Running %s...\n\n", filepath.Base(exePath))
	
	execCmd := exec.Command(exePath, execArgs...)
	execCmd.Dir = buildDir
	execCmd.Stdout = os.Stdout
	execCmd.Stderr = os.Stderr
	execCmd.Stdin = os.Stdin
	
	return execCmd.Run()
}

func GetRunCmd() *cobra.Command {
	return runCmd
}

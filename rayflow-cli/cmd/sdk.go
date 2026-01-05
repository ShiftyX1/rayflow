package cmd

import (
	"archive/tar"
	"archive/zip"
	"compress/gzip"
	"encoding/json"
	"fmt"
	"io"
	"net/http"
	"os"
	"path/filepath"
	"runtime"
	"strings"

	"github.com/spf13/cobra"
)

const (
	sdkBaseURL  = "https://github.com/ShiftyX1/rayflow/releases/download"
	releasesAPI = "https://api.github.com/repos/ShiftyX1/rayflow/releases"
)

type githubAsset struct {
	Name               string `json:"name"`
	BrowserDownloadURL string `json:"browser_download_url"`
}

type githubRelease struct {
	TagName    string        `json:"tag_name"`
	Name       string        `json:"name"`
	Draft      bool          `json:"draft"`
	Prerelease bool          `json:"prerelease"`
	Assets     []githubAsset `json:"assets"`
}

var sdkCmd = &cobra.Command{
	Use:   "sdk",
	Short: "Manage Rayflow SDK installations",
	Long: `Manage Rayflow SDK installations.

Commands:
  install [version]   Download and install SDK (latest if not specified)
  list                Show installed SDK versions
  available           Show available SDK versions from GitHub
  remove <version>    Remove an installed SDK version

Examples:
  rayflow-cli sdk install           # Install latest version
  rayflow-cli sdk install latest    # Install latest version
  rayflow-cli sdk install 0.4.2     # Install specific version
  rayflow-cli sdk list              # List installed versions
  rayflow-cli sdk available         # List available versions
  rayflow-cli sdk remove 0.4.2      # Remove a version`,
}

var sdkInstallCmd = &cobra.Command{
	Use:   "install [version]",
	Short: "Download and install Rayflow SDK",
	Long: `Download and install Rayflow SDK from GitHub releases.

If no version is specified or "latest" is used, installs the latest available version.

SDK will be installed to:
  - Linux/macOS: ~/.rayflow/sdk/<version>/
  - Windows: %LOCALAPPDATA%\rayflow\sdk\<version>\`,
	Args: cobra.MaximumNArgs(1),
	RunE: runSDKInstall,
}

var sdkListCmd = &cobra.Command{
	Use:   "list",
	Short: "List installed SDK versions",
	RunE:  runSDKList,
}

var sdkRemoveCmd = &cobra.Command{
	Use:   "remove <version>",
	Short: "Remove an installed SDK version",
	Args:  cobra.ExactArgs(1),
	RunE:  runSDKRemove,
}

var sdkAvailableCmd = &cobra.Command{
	Use:   "available",
	Short: "List available SDK versions from GitHub",
	Long: `Query GitHub releases API to show all available SDK versions
that have binaries for your current platform.`,
	RunE: runSDKAvailable,
}

func init() {
	sdkCmd.AddCommand(sdkInstallCmd)
	sdkCmd.AddCommand(sdkListCmd)
	sdkCmd.AddCommand(sdkRemoveCmd)
	sdkCmd.AddCommand(sdkAvailableCmd)
}

func GetSDKCmd() *cobra.Command {
	return sdkCmd
}

// GetRayflowDir returns the base rayflow directory
// Linux/macOS: ~/.rayflow
// Windows: %LOCALAPPDATA%\rayflow
func GetRayflowDir() string {
	if runtime.GOOS == "windows" {
		localAppData := os.Getenv("LOCALAPPDATA")
		if localAppData == "" {
			localAppData = filepath.Join(os.Getenv("USERPROFILE"), "AppData", "Local")
		}
		return filepath.Join(localAppData, "rayflow")
	}
	return filepath.Join(os.Getenv("HOME"), ".rayflow")
}

// GetSDKDir returns the SDK directory for a specific version
func GetSDKDir(version string) string {
	return filepath.Join(GetRayflowDir(), "sdk", version)
}

// GetSDKBaseDir returns the base SDK directory
func GetSDKBaseDir() string {
	return filepath.Join(GetRayflowDir(), "sdk")
}

// IsSDKInstalled checks if a specific SDK version is installed
func IsSDKInstalled(version string) bool {
	sdkDir := GetSDKDir(version)
	// Check for lib directory as indicator of valid installation
	libDir := filepath.Join(sdkDir, "lib")
	info, err := os.Stat(libDir)
	return err == nil && info.IsDir()
}

// =============================================================================
// Platform detection
// =============================================================================

type platformInfo struct {
	name      string // e.g., "linux-x64", "macos-arm64"
	extension string // ".tar.gz" or ".zip"
}

func getPlatformInfo() (*platformInfo, error) {
	var name string
	var ext string

	switch runtime.GOOS {
	case "linux":
		if runtime.GOARCH == "amd64" {
			name = "linux-x64"
		} else {
			return nil, fmt.Errorf("unsupported Linux architecture: %s", runtime.GOARCH)
		}
		ext = ".tar.gz"
	case "darwin":
		switch runtime.GOARCH {
		case "amd64":
			name = "macos-x64"
		case "arm64":
			name = "macos-arm64"
		default:
			return nil, fmt.Errorf("unsupported macOS architecture: %s", runtime.GOARCH)
		}
		ext = ".tar.gz"
	case "windows":
		if runtime.GOARCH == "amd64" {
			name = "windows-x64"
		} else {
			return nil, fmt.Errorf("unsupported Windows architecture: %s", runtime.GOARCH)
		}
		ext = ".zip"
	default:
		return nil, fmt.Errorf("unsupported operating system: %s", runtime.GOOS)
	}

	return &platformInfo{name: name, extension: ext}, nil
}

func getSDKDownloadURL(version string) (string, error) {
	platform, err := getPlatformInfo()
	if err != nil {
		return "", err
	}

	// https://github.com/ShiftyX1/rayflow/releases/download/v0.4.2/rayflow-sdk-macos-arm64.tar.gz
	filename := fmt.Sprintf("rayflow-sdk-%s%s", platform.name, platform.extension)
	return fmt.Sprintf("%s/v%s/%s", sdkBaseURL, version, filename), nil
}

func getSDKAssetName() (string, error) {
	platform, err := getPlatformInfo()
	if err != nil {
		return "", err
	}
	return fmt.Sprintf("rayflow-sdk-%s%s", platform.name, platform.extension), nil
}

// =============================================================================
// GitHub API
// =============================================================================

func fetchAvailableReleases() ([]githubRelease, error) {
	req, err := http.NewRequest("GET", releasesAPI, nil)
	if err != nil {
		return nil, fmt.Errorf("failed to create request: %w", err)
	}

	req.Header.Set("Accept", "application/vnd.github.v3+json")
	req.Header.Set("User-Agent", "rayflow-cli")

	resp, err := http.DefaultClient.Do(req)
	if err != nil {
		return nil, fmt.Errorf("failed to fetch releases: %w", err)
	}
	defer resp.Body.Close()

	if resp.StatusCode != http.StatusOK {
		return nil, fmt.Errorf("GitHub API returned status: %s", resp.Status)
	}

	var releases []githubRelease
	if err := json.NewDecoder(resp.Body).Decode(&releases); err != nil {
		return nil, fmt.Errorf("failed to parse releases: %w", err)
	}

	return releases, nil
}

// SDKVersion represents an available SDK version
type SDKVersion struct {
	Version     string
	TagName     string
	DownloadURL string
	Prerelease  bool
}

// GetAvailableSDKVersions returns all SDK versions available for the current platform
func GetAvailableSDKVersions() ([]SDKVersion, error) {
	assetName, err := getSDKAssetName()
	if err != nil {
		return nil, err
	}

	releases, err := fetchAvailableReleases()
	if err != nil {
		return nil, err
	}

	var versions []SDKVersion
	for _, release := range releases {
		if release.Draft {
			continue
		}

		// Check if this release has SDK for our platform
		for _, asset := range release.Assets {
			if asset.Name == assetName {
				version := strings.TrimPrefix(release.TagName, "v")
				versions = append(versions, SDKVersion{
					Version:     version,
					TagName:     release.TagName,
					DownloadURL: asset.BrowserDownloadURL,
					Prerelease:  release.Prerelease,
				})
				break
			}
		}
	}

	return versions, nil
}

// GetLatestSDKVersion returns the latest stable SDK version for the current platform
func GetLatestSDKVersion() (*SDKVersion, error) {
	versions, err := GetAvailableSDKVersions()
	if err != nil {
		return nil, err
	}

	// Find first non-prerelease version (releases are sorted by date, newest first)
	for _, v := range versions {
		if !v.Prerelease {
			return &v, nil
		}
	}

	// If no stable releases, return the first prerelease
	if len(versions) > 0 {
		return &versions[0], nil
	}

	return nil, fmt.Errorf("no SDK releases found for your platform")
}

// =============================================================================
// Download and extraction
// =============================================================================

func downloadFile(url string, destPath string) error {
	fmt.Printf("Downloading %s...\n", url)

	resp, err := http.Get(url)
	if err != nil {
		return fmt.Errorf("failed to download: %w", err)
	}
	defer resp.Body.Close()

	if resp.StatusCode != http.StatusOK {
		return fmt.Errorf("download failed with status: %s", resp.Status)
	}

	out, err := os.Create(destPath)
	if err != nil {
		return fmt.Errorf("failed to create file: %w", err)
	}
	defer out.Close()

	// Show progress for large files
	contentLength := resp.ContentLength
	var written int64
	buf := make([]byte, 32*1024)

	for {
		nr, er := resp.Body.Read(buf)
		if nr > 0 {
			nw, ew := out.Write(buf[0:nr])
			if nw < 0 || nr < nw {
				nw = 0
				if ew == nil {
					ew = fmt.Errorf("invalid write result")
				}
			}
			written += int64(nw)
			if ew != nil {
				err = ew
				break
			}
			if nr != nw {
				err = io.ErrShortWrite
				break
			}

			if contentLength > 0 {
				percent := float64(written) / float64(contentLength) * 100
				fmt.Printf("\rDownloading... %.1f%%", percent)
			}
		}
		if er != nil {
			if er != io.EOF {
				err = er
			}
			break
		}
	}
	fmt.Println()

	return err
}

func extractTarGz(archivePath, destDir string) error {
	fmt.Printf("Extracting to %s...\n", destDir)

	file, err := os.Open(archivePath)
	if err != nil {
		return fmt.Errorf("failed to open archive: %w", err)
	}
	defer file.Close()

	gzr, err := gzip.NewReader(file)
	if err != nil {
		return fmt.Errorf("failed to create gzip reader: %w", err)
	}
	defer gzr.Close()

	tr := tar.NewReader(gzr)

	for {
		header, err := tr.Next()
		if err == io.EOF {
			break
		}
		if err != nil {
			return fmt.Errorf("tar read error: %w", err)
		}

		// Remove the top-level directory from the path (e.g., "rayflow-sdk-macos-arm64/")
		parts := strings.SplitN(header.Name, "/", 2)
		var targetPath string
		if len(parts) > 1 {
			targetPath = filepath.Join(destDir, parts[1])
		} else {
			continue // Skip the root directory entry
		}

		if targetPath == destDir {
			continue
		}

		switch header.Typeflag {
		case tar.TypeDir:
			if err := os.MkdirAll(targetPath, 0755); err != nil {
				return fmt.Errorf("failed to create directory: %w", err)
			}
		case tar.TypeReg:
			if err := os.MkdirAll(filepath.Dir(targetPath), 0755); err != nil {
				return fmt.Errorf("failed to create parent directory: %w", err)
			}

			outFile, err := os.OpenFile(targetPath, os.O_CREATE|os.O_RDWR|os.O_TRUNC, os.FileMode(header.Mode))
			if err != nil {
				return fmt.Errorf("failed to create file: %w", err)
			}

			if _, err := io.Copy(outFile, tr); err != nil {
				outFile.Close()
				return fmt.Errorf("failed to write file: %w", err)
			}
			outFile.Close()
		case tar.TypeSymlink:
			if err := os.Symlink(header.Linkname, targetPath); err != nil {
				// Ignore symlink errors on Windows
				if runtime.GOOS != "windows" {
					return fmt.Errorf("failed to create symlink: %w", err)
				}
			}
		}
	}

	return nil
}

func extractZip(archivePath, destDir string) error {
	fmt.Printf("Extracting to %s...\n", destDir)

	r, err := zip.OpenReader(archivePath)
	if err != nil {
		return fmt.Errorf("failed to open zip: %w", err)
	}
	defer r.Close()

	for _, f := range r.File {
		// Remove the top-level directory from the path
		parts := strings.SplitN(f.Name, "/", 2)
		var targetPath string
		if len(parts) > 1 {
			targetPath = filepath.Join(destDir, parts[1])
		} else {
			continue
		}

		if targetPath == destDir {
			continue
		}

		if f.FileInfo().IsDir() {
			if err := os.MkdirAll(targetPath, 0755); err != nil {
				return fmt.Errorf("failed to create directory: %w", err)
			}
			continue
		}

		if err := os.MkdirAll(filepath.Dir(targetPath), 0755); err != nil {
			return fmt.Errorf("failed to create parent directory: %w", err)
		}

		outFile, err := os.OpenFile(targetPath, os.O_CREATE|os.O_RDWR|os.O_TRUNC, f.Mode())
		if err != nil {
			return fmt.Errorf("failed to create file: %w", err)
		}

		rc, err := f.Open()
		if err != nil {
			outFile.Close()
			return fmt.Errorf("failed to open file in zip: %w", err)
		}

		_, err = io.Copy(outFile, rc)
		rc.Close()
		outFile.Close()

		if err != nil {
			return fmt.Errorf("failed to write file: %w", err)
		}
	}

	return nil
}

// =============================================================================
// Command implementations
// =============================================================================

func runSDKInstall(cmd *cobra.Command, args []string) error {
	var version string
	var downloadURL string

	if len(args) == 0 || args[0] == "latest" {
		// Fetch latest version from GitHub
		fmt.Println("Fetching available versions...")
		latest, err := GetLatestSDKVersion()
		if err != nil {
			return fmt.Errorf("failed to get latest version: %w", err)
		}
		version = latest.Version
		downloadURL = latest.DownloadURL
		fmt.Printf("Latest version: %s\n\n", version)
	} else {
		version = args[0]
	}

	// Check if already installed
	if IsSDKInstalled(version) {
		fmt.Printf("SDK version %s is already installed at %s\n", version, GetSDKDir(version))
		return nil
	}

	// Get platform info
	platform, err := getPlatformInfo()
	if err != nil {
		return err
	}

	// Get download URL if not already set
	if downloadURL == "" {
		downloadURL, err = getSDKDownloadURL(version)
		if err != nil {
			return err
		}
	}

	// Create temp directory for download
	tempDir, err := os.MkdirTemp("", "rayflow-sdk-")
	if err != nil {
		return fmt.Errorf("failed to create temp directory: %w", err)
	}
	defer os.RemoveAll(tempDir)

	// Download
	archivePath := filepath.Join(tempDir, fmt.Sprintf("rayflow-sdk-%s%s", platform.name, platform.extension))
	if err := downloadFile(downloadURL, archivePath); err != nil {
		return err
	}

	// Create destination directory
	destDir := GetSDKDir(version)
	if err := os.MkdirAll(destDir, 0755); err != nil {
		return fmt.Errorf("failed to create SDK directory: %w", err)
	}

	// Extract
	if platform.extension == ".zip" {
		if err := extractZip(archivePath, destDir); err != nil {
			os.RemoveAll(destDir)
			return err
		}
	} else {
		if err := extractTarGz(archivePath, destDir); err != nil {
			os.RemoveAll(destDir)
			return err
		}
	}

	fmt.Printf("\n✓ SDK %s installed successfully to %s\n", version, destDir)
	return nil
}

func runSDKList(cmd *cobra.Command, args []string) error {
	sdkBaseDir := GetSDKBaseDir()

	entries, err := os.ReadDir(sdkBaseDir)
	if err != nil {
		if os.IsNotExist(err) {
			fmt.Println("No SDK versions installed.")
			fmt.Println("\nInstall SDK with: rayflow-cli sdk install")
			return nil
		}
		return fmt.Errorf("failed to read SDK directory: %w", err)
	}

	if len(entries) == 0 {
		fmt.Println("No SDK versions installed.")
		fmt.Println("\nInstall SDK with: rayflow-cli sdk install")
		return nil
	}

	fmt.Println("Installed SDK versions:")
	for _, entry := range entries {
		if entry.IsDir() {
			sdkDir := filepath.Join(sdkBaseDir, entry.Name())
			// Verify it's a valid SDK installation
			if _, err := os.Stat(filepath.Join(sdkDir, "lib")); err == nil {
				fmt.Printf("  %s  (%s)\n", entry.Name(), sdkDir)
			}
		}
	}

	return nil
}

func runSDKRemove(cmd *cobra.Command, args []string) error {
	version := args[0]
	sdkDir := GetSDKDir(version)

	if !IsSDKInstalled(version) {
		return fmt.Errorf("SDK version %s is not installed", version)
	}

	fmt.Printf("Removing SDK %s from %s...\n", version, sdkDir)

	if err := os.RemoveAll(sdkDir); err != nil {
		return fmt.Errorf("failed to remove SDK: %w", err)
	}

	fmt.Printf("✓ SDK %s removed successfully\n", version)
	return nil
}

func runSDKAvailable(cmd *cobra.Command, args []string) error {
	platform, err := getPlatformInfo()
	if err != nil {
		return err
	}

	fmt.Printf("Fetching available SDK versions for %s...\n\n", platform.name)

	versions, err := GetAvailableSDKVersions()
	if err != nil {
		return err
	}

	if len(versions) == 0 {
		fmt.Println("No SDK versions available for your platform.")
		return nil
	}

	fmt.Println("Available SDK versions:")
	for i, v := range versions {
		status := ""
		if IsSDKInstalled(v.Version) {
			status = " (installed)"
		}
		if v.Prerelease {
			status += " [prerelease]"
		}
		if i == 0 {
			status += " [latest]"
		}
		fmt.Printf("  %s%s\n", v.Version, status)
	}

	fmt.Println("\nInstall with: rayflow-cli sdk install <version>")
	fmt.Println("              rayflow-cli sdk install         # installs latest")
	return nil
}

package main

import (
	"flag"
	"fmt"
	"log"
)

// ─── CLI 入口：blessstar schema bundle ───

func main() {
	schemaDir := flag.String("dir", "config-schema.d", "Path to config-schema.d/ directory")
	output := flag.String("output", "config-schema.bundled.yaml", "Output path for bundled.yaml")
	checkOnly := flag.Bool("check", false, "CI mode: validate only, do not write output")
	flag.Parse()

	if *schemaDir == "" {
		log.Fatal("--dir is required")
	}

	fmt.Println("🔗 Schema Bundler: aggregating SSOT files...")
	fmt.Printf("   Input dir:  %s\n", *schemaDir)
	fmt.Printf("   Output:     %s\n", *output)
	if *checkOnly {
		fmt.Println("   Mode:       CHECK only (no output)")
	}

	// Step 1: Scan directory for *.yaml files
	yamlFiles, err := scanYAMLFiles(*schemaDir)
	if err != nil {
		log.Fatalf("Failed to scan directory %s: %v", *schemaDir, err)
	}
	if len(yamlFiles) == 0 {
		log.Fatalf("No YAML files found in %s", *schemaDir)
	}

	fmt.Printf("   Found %d SSOT file(s):\n", len(yamlFiles))
	for _, f := range yamlFiles {
		fmt.Printf("     - %s\n", f)
	}

	// Step 2: Parse each YAML file
	schemas, err := parseAllSchemas(*schemaDir, yamlFiles)
	if err != nil {
		log.Fatalf("Failed to parse SSOT files: %v", err)
	}

	// Step 3: Validate each file independently (Gold Standard per-file)
	if err := validateEachSchema(schemas); err != nil {
		log.Fatalf("Gold Standard validation failed: %v", err)
	}
	fmt.Println("   ✅ Per-file Gold Standard validation passed")

	// Step 4: Resolve cross-file dependencies
	if err := resolveCrossFileDeps(schemas); err != nil {
		log.Fatalf("Cross-file dependency resolution failed: %v", err)
	}
	fmt.Println("   ✅ Cross-file dependency resolution passed")

	// Step 5: Merge into single bundled schema
	bundled, err := mergeSchemas(schemas)
	if err != nil {
		log.Fatalf("Merge failed: %v", err)
	}

	// Step 6: Validate merged result (Gold Standard global)
	if err := validateBundled(bundled); err != nil {
		log.Fatalf("Global Gold Standard validation failed: %v", err)
	}
	fmt.Println("   ✅ Global Gold Standard validation passed")

	// Step 7: Output
	if *checkOnly {
		fmt.Println("\n✅ All checks passed. Bundled schema is valid.")
		return
	}

	if err := writeBundledYAML(bundled, *output); err != nil {
		log.Fatalf("Failed to write bundled YAML: %v", err)
	}
	fmt.Printf("\n✅ Bundled schema written to %s (%d fields)\n", *output, len(bundled.Fields))
}

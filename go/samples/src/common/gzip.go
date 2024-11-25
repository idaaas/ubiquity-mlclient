package common

import (
	"compress/gzip"
	"io"
	"log"
	"os"
)

func Unzip(inputPath string, outputPath string) {

	// Open the .gz file
	inputFile, err := os.Open(inputPath)
	if err != nil {
		log.Fatalf("Failed to open input file: %v", err)
	}
	defer inputFile.Close()

	// Create a gzip reader
	gzipReader, err := gzip.NewReader(inputFile)
	if err != nil {
		log.Fatalf("Failed to create gzip reader: %v", err)
	}
	defer gzipReader.Close()

	// Create the output file
	outputFile, err := os.Create(outputPath)
	if err != nil {
		log.Fatalf("Failed to create output file: %v", err)
	}
	defer outputFile.Close()

	// Copy the uncompressed data to the output file
	_, err = io.Copy(outputFile, gzipReader)
	if err != nil {
		log.Fatalf("Failed to write uncompressed data: %v", err)
	}

	log.Println("File successfully uncompressed.")

}

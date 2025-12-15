#include "AVIParser.h"
#include <Arduino.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  char chunkId[4];
  unsigned int chunkSize;
} ChunkHeader;

void readChunk(FILE *file, ChunkHeader *header) {
  fread(&header->chunkId, 4, 1, file);
  fread(&header->chunkSize, 4, 1, file);
  // Serial.printf("ChunkId %c%c%c%c, size %u\n",
  //        header->chunkId[0], header->chunkId[1],
  //        header->chunkId[2], header->chunkId[3],
  //        header->chunkSize);
}

AVIParser::AVIParser(std::string fname, AVIChunkType requiredChunkType)
    : mFileName(fname), mRequiredChunkType(requiredChunkType) {}

AVIParser::~AVIParser() {
  if (mFile) {
    fclose(mFile);
  }
}

// http://www.fastgraph.com/help/avi_header_format.html
typedef struct {
  unsigned int dwMicroSecPerFrame;
  unsigned int dwMaxBytesPerSec;
  unsigned int dwPaddingGranularity;
  unsigned int dwFlags;
  unsigned int dwTotalFrames;
  unsigned int dwInitialFrames;
  unsigned int dwStreams;
  unsigned int dwSuggestedBufferSize;
  unsigned int dwWidth;
  unsigned int dwHeight;
  unsigned int dwScale;
  unsigned int dwRate;
  unsigned int dwStart;
  unsigned int dwLength;
} MainAVIHeader;

// http://www.fastgraph.com/help/avi_header_format.html
typedef struct {
  char fccType[4];
  char fccHandler[4];
  unsigned int dwFlags;
  unsigned short wPriority;
  unsigned short wLanguage;
  unsigned int dwInitialFrames;
  unsigned int dwScale;
  unsigned int dwRate;
  unsigned int dwStart;
  unsigned int dwLength;
  unsigned int dwSuggestedBufferSize;
} AVIStreamHeader;

bool AVIParser::open() {
  mFile = fopen(mFileName.c_str(), "rb");
  if (!mFile) {
    Serial.printf("Failed to open file.\n");
    return false;
  }
  // check the file is valid
  ChunkHeader header;
  // Read RIFF header
  readChunk(mFile, &header);
  if (strncmp(header.chunkId, "RIFF", 4) != 0) {
    Serial.println("Not a valid AVI file.");
    fclose(mFile);
    mFile = NULL;
    return false;
  }
  // next four bytes are the RIFF type which should be 'AVI '
  char riffType[4];
  fread(&riffType, 4, 1, mFile);
  if (strncmp(riffType, "AVI ", 4) != 0) {
    Serial.println("Not a valid AVI file.");
    fclose(mFile);
    mFile = NULL;
    return false;
  }

  // now read each chunk and find the movi list
  while (!feof(mFile) && !ferror(mFile)) {
    readChunk(mFile, &header);
    if (feof(mFile) || ferror(mFile)) {
      break;
    }
    // is it a LIST chunk?
    if (strncmp(header.chunkId, "LIST", 4) == 0) {
      long listContentPosition = ftell(mFile);
      char listType[4];
      fread(&listType, 4, 1, mFile);

      if (strncmp(listType, "hdrl", 4) == 0) {
        // We are inside the 'hdrl' LIST chunk. Its content starts at
        // ftell(mFile) and ends at listContentPosition + header.chunkSize.
        long hdrlContentRemaining =
            header.chunkSize - 4; // -4 for 'hdrl' type already read

        while (hdrlContentRemaining > 0 && !feof(mFile) && !ferror(mFile)) {
          ChunkHeader subHeader;
          long bytesReadForSubHeader =
              fread(&subHeader, 1, sizeof(ChunkHeader), mFile);
          if (bytesReadForSubHeader != sizeof(ChunkHeader)) {
            // Error or EOF
            break;
          }
          hdrlContentRemaining -= bytesReadForSubHeader;

          long subChunkDataSize = subHeader.chunkSize;
          long subChunkTotalSize = subChunkDataSize;
          if (subChunkTotalSize % 2 != 0) { // Account for padding
            subChunkTotalSize++;
          }

          // Process the sub-chunk
          if (strncmp(subHeader.chunkId, "avih", 4) == 0) {
            // We don't need to read avih content.
            fseek(mFile, subChunkDataSize, SEEK_CUR);
            hdrlContentRemaining -= subChunkTotalSize;
          } else if (strncmp(subHeader.chunkId, "LIST", 4) == 0) {
            char subListType[4];
            long bytesReadForSubListType = fread(&subListType, 1, 4, mFile);
            if (bytesReadForSubListType != 4) {
              // Error or EOF
              break;
            }
            subChunkDataSize -=
                bytesReadForSubListType; // Account for 'strl' type read

            if (strncmp(subListType, "strl", 4) == 0) {
              long strlContentRemaining = subChunkDataSize;
              while (strlContentRemaining > 0 && !feof(mFile) &&
                     !ferror(mFile)) {
                ChunkHeader strhHeader;
                long bytesReadForStrhHeader =
                    fread(&strhHeader, 1, sizeof(ChunkHeader), mFile);
                if (bytesReadForStrhHeader != sizeof(ChunkHeader)) {
                  // Error or EOF
                  break;
                }
                strlContentRemaining -= bytesReadForStrhHeader;

                long strhDataSize = strhHeader.chunkSize;
                long strhTotalSize = strhDataSize;
                if (strhTotalSize % 2 != 0) { // Account for padding
                  strhTotalSize++;
                }

                if (strncmp(strhHeader.chunkId, "strh", 4) == 0) {
                  AVIStreamHeader strh;
                  long bytesReadForStrh =
                      fread(&strh, 1, sizeof(AVIStreamHeader), mFile);
                  if (bytesReadForStrh != sizeof(AVIStreamHeader)) {
                    // Error or EOF
                    break;
                  }
                  strhDataSize -=
                      bytesReadForStrh; // Account for strh struct read

                  if (strncmp(strh.fccType, "vids", 4) == 0) {
                    if (strh.dwScale == 0) {
                      Serial.println(
                          "Warning: dwScale is 0, can't calculate framerate.");
                      mFrameRate = 0;
                    } else {
                      mFrameRate = (float)strh.dwRate / strh.dwScale;
                      Serial.printf("Frame rate: %f\n", mFrameRate);
                    }
                  }
                  fseek(mFile, strhDataSize,
                        SEEK_CUR); // Skip remaining strh data
                  strlContentRemaining -= strhTotalSize;
                } else {
                  // Not 'strh', skip its content
                  fseek(mFile, strhDataSize, SEEK_CUR);
                  strlContentRemaining -= strhTotalSize;
                }
              }
              hdrlContentRemaining -=
                  subChunkTotalSize; // All of 'strl' content handled
            } else {
              // Not 'strl', skip the rest of this LIST chunk's content
              fseek(mFile, subChunkDataSize, SEEK_CUR);
              hdrlContentRemaining -= subChunkTotalSize;
            }
          } else {
            // Not 'avih' or 'LIST', skip its content
            fseek(mFile, subChunkDataSize, SEEK_CUR);
            hdrlContentRemaining -= subChunkTotalSize;
          }
        }
      } else if (strncmp(listType, "movi", 4) == 0) {
        // This is the movie list. We've found what we're looking for.
        Serial.printf("Found movi list.\n");
        mMoviListPosition =
            ftell(mFile); // The current position is the start of the movi data
        mMoviListLength = header.chunkSize - 4;
        Serial.printf("List Chunk Length: %ld\n", mMoviListLength);
        // We can stop parsing the file now.
        break;
      } else {
        // This is some other kind of LIST chunk that we don't care about. Skip
        // it.
        fseek(mFile, header.chunkSize - 4, SEEK_CUR);
        if (header.chunkSize % 2 != 0) {
          fseek(mFile, 1, SEEK_CUR);
        }
      }
    } else {
      // This is not a LIST chunk. Skip it.
      fseek(mFile, header.chunkSize, SEEK_CUR);
      if (header.chunkSize % 2 != 0) {
        fseek(mFile, 1, SEEK_CUR);
      }
    }
  }

  if (mMoviListPosition == 0) {
    Serial.printf("Failed to find the movi list.\n");
    fclose(mFile);
    mFile = NULL;
    return false;
  }

  // Before we return, we must position the file pointer at the start of the
  // movi data.
  fseek(mFile, mMoviListPosition, SEEK_SET);
  return true;
}

size_t AVIParser::getNextChunk(uint8_t **buffer, size_t &bufferLength) {
  // check if the file is open
  if (!mFile) {
    Serial.println("No file open.");
    return 0;
  }
  // did we find the movi list?
  if (mMoviListPosition == 0) {
    Serial.println("No movi list found.");
    return 0;
  }
  // get the next chunk of data from the list
  ChunkHeader header;
  while (mMoviListLength > 0) {
    readChunk(mFile, &header);
    mMoviListLength -= 8;

    static uint32_t dbgChunkPrints = 0;
    if (dbgChunkPrints < 12) {
      Serial.printf("movi chunk %c%c%c%c size=%u\n", header.chunkId[0],
                    header.chunkId[1], header.chunkId[2], header.chunkId[3],
                    header.chunkSize);
      dbgChunkPrints++;
    }

    if (strncmp(header.chunkId, "LIST", 4) == 0) {
      char listType[4];
      if (fread(&listType, 4, 1, mFile) != 1) {
        return 0;
      }
      mMoviListLength -= 4;
      long listRemaining = (long)header.chunkSize - 4;

      if (strncmp(listType, "rec ", 4) == 0) {
        while (listRemaining > 0 && mMoviListLength > 0) {
          ChunkHeader subHeader;
          readChunk(mFile, &subHeader);
          listRemaining -= 8;
          mMoviListLength -= 8;

          bool isVideoChunk = strncmp(subHeader.chunkId, "00dc", 4) == 0;
          bool isAudioChunk =
              (subHeader.chunkId[2] == 'w' && subHeader.chunkId[3] == 'b');
          isVideoChunk =
              (subHeader.chunkId[2] == 'd' &&
               (subHeader.chunkId[3] == 'c' || subHeader.chunkId[3] == 'b'));
          if (mRequiredChunkType == AVIChunkType::VIDEO && isVideoChunk ||
              mRequiredChunkType == AVIChunkType::AUDIO && isAudioChunk) {
            if (subHeader.chunkSize == 0) {
              continue;
            }
            if (subHeader.chunkSize > bufferLength) {
              uint8_t *newBuf =
                  (uint8_t *)realloc(*buffer, subHeader.chunkSize);
              if (!newBuf) {
                Serial.printf("realloc failed for chunk size=%u\n",
                              subHeader.chunkSize);
                return 0;
              }
              *buffer = newBuf;
              bufferLength = subHeader.chunkSize;
            }
            if (fread(*buffer, subHeader.chunkSize, 1, mFile) != 1) {
              Serial.printf("fread failed for chunk size=%u\n",
                            subHeader.chunkSize);
              Serial.printf("fread failed for chunk size=%u\n",
                            subHeader.chunkSize);
              return 0;
            }
            listRemaining -= subHeader.chunkSize;
            mMoviListLength -= subHeader.chunkSize;
            if (subHeader.chunkSize % 2 != 0) {
              fseek(mFile, 1, SEEK_CUR);
              listRemaining--;
              mMoviListLength--;
            }
            return subHeader.chunkSize;
          } else {
            fseek(mFile, subHeader.chunkSize, SEEK_CUR);
            listRemaining -= subHeader.chunkSize;
            mMoviListLength -= subHeader.chunkSize;
          }
          if (subHeader.chunkSize % 2 != 0) {
            fseek(mFile, 1, SEEK_CUR);
            listRemaining--;
            mMoviListLength--;
          }
        }
      } else {
        fseek(mFile, listRemaining, SEEK_CUR);
        mMoviListLength -= listRemaining;
      }

      if (header.chunkSize % 2 != 0) {
        fseek(mFile, 1, SEEK_CUR);
        mMoviListLength--;
      }
      continue;
    }

    bool isAudioChunk = (header.chunkId[2] == 'w' && header.chunkId[3] == 'b');
    bool isVideoChunk =
        (header.chunkId[2] == 'd' &&
         (header.chunkId[3] == 'c' || header.chunkId[3] == 'b'));
    if (mRequiredChunkType == AVIChunkType::VIDEO && isVideoChunk ||
        mRequiredChunkType == AVIChunkType::AUDIO && isAudioChunk) {
      // we've got the required chunk - copy it into the provided buffer
      // reallocate the buffer if necessary
      if (header.chunkSize == 0) {
        continue;
      }
      if (header.chunkSize > bufferLength) {
        uint8_t *newBuf = (uint8_t *)realloc(*buffer, header.chunkSize);
        if (!newBuf) {
          Serial.printf("realloc failed for chunk size=%u\n", header.chunkSize);
          return 0;
        }
        *buffer = newBuf;
        bufferLength = header.chunkSize;
      }
      // copy the chunk data
      if (fread(*buffer, header.chunkSize, 1, mFile) != 1) {
        Serial.printf("fread failed for chunk size=%u\n", header.chunkSize);
        return 0;
      }
      mMoviListLength -= header.chunkSize;
      // handle any padding bytes
      if (header.chunkSize % 2 != 0) {
        fseek(mFile, 1, SEEK_CUR);
        mMoviListLength--;
      }
      return header.chunkSize;
    } else {
      // the data is not what was required - skip over the chunk
      fseek(mFile, header.chunkSize, SEEK_CUR);
      mMoviListLength -= header.chunkSize;
    }
    // handle any padding bytes
    if (header.chunkSize % 2 != 0) {
      fseek(mFile, 1, SEEK_CUR);
      mMoviListLength--;
    }
  }
  // no more chunks
  Serial.println("No more data");
  return 0;
}
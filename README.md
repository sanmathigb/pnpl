# pnpl. Push Now Pop Later.

## Building

1. Clone the repository with submodules:
```bash
git clone --recursive https://github.com/yourusername/pnpl.git
cd pnpl
```

If you've already cloned without --recursive, initialize submodules:
```bash
git submodule update --init --recursive
```

2. Build the project:
```bash
mkdir build
cd build
cmake ..
make
```

## Usage

1. Download a model (example):
```bash
mkdir -p models
wget https://huggingface.co/TheBloke/Llama-2-7B-Chat-GGUF/resolve/main/llama-2-7b-chat.Q4_K_M.gguf -O models/llama-2-7b-chat.gguf
```

2. Run inference:
```bash
# Push a task
./build/pnpl push "Your Input to the LLM"

# Get the result
./build/pnpl pop
```

## Development

### Updating Dependencies

To update llama.cpp to the latest version:
```bash
git submodule update --remote third_party/llama.cpp
```

### Adding New Dependencies

1. Add the submodule:
```bash
git submodule add <repository-url> third_party/<dependency-name>
```

2. Update CMakeLists.txt to include the new dependency
3. Commit the changes:
```bash
git add .gitmodules third_party/
git commit -m "Add <dependency-name> as submodule"
```

## License

MIT License - see LICENSE file for details

## Features

- Simple, git-like command interface
- Asynchronous inference processing
- File-based job queue
- Support for custom prompts
- High-performance C++ implementation

## Quick Start

```bash
# Push content for inference
pnpl push "your content here" "summarize this"

# List completed jobs
pnpl list

# Get latest result
pnpl pop
```

## Project Structure

```
pnpl/
├── include/           # Public headers
│   └── pnpl/         # Library interface
├── src/              # Implementation
├── test/             # Unit tests
├── data/             # Input queue
└── results/          # Output queue
```

## Contributing

Please read [CONTRIBUTING.md](CONTRIBUTING.md) for details on our code of conduct and the process for submitting pull requests.

## Acknowledgments

- llama.cpp for the inference engine
- The open source community

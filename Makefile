CXX := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -Iinclude -g

SRC_DIR := src
OBJ_DIR := build/obj
BIN_DIR := build/bin

SRC_FILES := $(wildcard $(SRC_DIR)/*.cpp)
OBJ_FILES := $(patsubst $(SRC_DIR)/%.cpp,$(OBJ_DIR)/%.o,$(SRC_FILES))

TARGET := $(BIN_DIR)/mas

# 跨平台 mkdir
ifeq ($(OS),Windows_NT)
    define MKDIR
        if not exist "$(1)" mkdir "$(1)"
    endef
else
    define MKDIR
        mkdir -p $(1)
    endef
endif


all: $(TARGET)
	@echo "Build complete: $(TARGET)"
# 生成目标文件目录
$(OBJ_DIR):
	$(call MKDIR,$(OBJ_DIR))

$(BIN_DIR):
	$(call MKDIR,$(BIN_DIR))

$(TARGET): $(OBJ_FILES) | $(BIN_DIR)
	$(CXX) $(OBJ_FILES) -o $@

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# 清理
ifeq ($(OS),Windows_NT)
clean:
	@if exist "$(OBJ_DIR)" rmdir /s /q "$(OBJ_DIR)"
	@if exist "$(BIN_DIR)" rmdir /s /q "$(BIN_DIR)"
else
clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)
endif

rebuild: clean all

.PHONY: all clean rebuild
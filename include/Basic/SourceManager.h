#ifndef RCC_BASIC_SOURCEMANAGER_H
#define RCC_BASIC_SOURCEMANAGER_H

namespace rcc {

class SourceManager {
public:
  SourceManager() = default;

  SourceManager(char *Start) : Start(Start) {}

  char *getStart() const { return Start; }

  void setStart(char *Start) { this->Start = Start; }

private:
  char *Start = nullptr;
};

} // namespace rcc

#endif
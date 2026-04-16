#pragma once

#include "SceneCommand.h"

#include <vector>

namespace Nova {

struct CommandHistoryEntry {
    SceneCommand Undo;
    SceneCommand Redo;
};

class SceneCommandHistory {
public:
    void Record(const SceneCommand& undoCommand, const SceneCommand& redoCommand);

    bool CanUndo() const;
    bool CanRedo() const;

    CommandHistoryEntry PopUndo();
    CommandHistoryEntry PopRedo();

    void PushRedo(const CommandHistoryEntry& entry);
    void PushUndo(const CommandHistoryEntry& entry);
    void Clear();

private:
    std::vector<CommandHistoryEntry> m_UndoStack;
    std::vector<CommandHistoryEntry> m_RedoStack;
};

} // namespace Nova

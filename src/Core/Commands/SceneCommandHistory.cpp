#include "SceneCommandHistory.h"

namespace Nova {

void SceneCommandHistory::Record(const SceneCommand& undoCommand, const SceneCommand& redoCommand) {
    m_UndoStack.push_back({ undoCommand, redoCommand });
    m_RedoStack.clear();
}

bool SceneCommandHistory::CanUndo() const {
    return !m_UndoStack.empty();
}

bool SceneCommandHistory::CanRedo() const {
    return !m_RedoStack.empty();
}

CommandHistoryEntry SceneCommandHistory::PopUndo() {
    CommandHistoryEntry entry = m_UndoStack.back();
    m_UndoStack.pop_back();
    return entry;
}

CommandHistoryEntry SceneCommandHistory::PopRedo() {
    CommandHistoryEntry entry = m_RedoStack.back();
    m_RedoStack.pop_back();
    return entry;
}

void SceneCommandHistory::PushRedo(const CommandHistoryEntry& entry) {
    m_RedoStack.push_back(entry);
}

void SceneCommandHistory::PushUndo(const CommandHistoryEntry& entry) {
    m_UndoStack.push_back(entry);
}

void SceneCommandHistory::Clear() {
    m_UndoStack.clear();
    m_RedoStack.clear();
}

} // namespace Nova

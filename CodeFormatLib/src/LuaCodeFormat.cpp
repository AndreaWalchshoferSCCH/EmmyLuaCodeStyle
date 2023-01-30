#include "LuaCodeFormat.h"

#include "CodeService/Config/LuaEditorConfig.h"
#include "Util/StringUtil.h"
#include "LuaParser/Parse/LuaParser.h"
#include "CodeService/RangeFormat/RangeFormatBuilder.h"

LuaCodeFormat &LuaCodeFormat::GetInstance() {
    static LuaCodeFormat instance;
    return instance;
}

LuaCodeFormat::LuaCodeFormat()
        :
        _spellChecker(std::make_shared<CodeSpellChecker>()) {
}

void LuaCodeFormat::UpdateCodeStyle(const std::string &workspaceUri, const std::string &configPath) {
    for (auto &config: _configs) {
        if (config.Workspace == workspaceUri) {
            config.Editorconfig = LuaEditorConfig::LoadFromFile(configPath);
            config.Editorconfig->Parse();
            return;
        }
    }

    auto &config = _configs.emplace_back(
            workspaceUri
    );
    config.Editorconfig = LuaEditorConfig::LoadFromFile(configPath);
    config.Editorconfig->Parse();
}

void LuaCodeFormat::RemoveCodeStyle(const std::string &workspaceUri) {
    for (auto it = _configs.begin(); it != _configs.end(); it++) {
        if (it->Workspace == workspaceUri) {
            _configs.erase(it);
            break;
        }
    }
}

void LuaCodeFormat::SetDefaultCodeStyle(ConfigMap &configMap) {
    if (!configMap.empty()) {
        LuaStyle style;
        style.ParseFromMap(configMap);
        _defaultStyle = style;
    }
}

void LuaCodeFormat::SetSupportNonStandardSymbol(const std::string &tokenType, const std::vector<std::string> &tokens) {
//    if (tokenType.size() == 1) {
//        _customParser->SetTokens(tokenType.front(), tokens);
//    }
}

void LuaCodeFormat::LoadSpellDictionary(const std::string &path) {
    _spellChecker->LoadDictionary(path);
}

void LuaCodeFormat::LoadSpellDictionaryFromBuffer(const std::string &buffer) {
    _spellChecker->LoadDictionaryFromBuffer(buffer);
}

std::string LuaCodeFormat::Reformat(const std::string &uri, std::string &&text, ConfigMap &configMap) {
    auto file = std::make_shared<LuaFile>(std::move(text));
    LuaLexer luaLexer(file);
    luaLexer.Parse();

    LuaParser p(file, std::move(luaLexer.GetTokens()));
    p.Parse();

    LuaSyntaxTree t;
    t.BuildTree(p);

    LuaStyle style = GetStyle(uri);
    FormatBuilder f(style);

    return f.GetFormatResult(t);
}

std::string LuaCodeFormat::RangeFormat(const std::string &uri, FormatRange &range,
                                       std::string &&text,
                                       ConfigMap &configMap) {
    auto file = std::make_shared<LuaFile>(std::move(text));
    LuaLexer luaLexer(file);
    luaLexer.Parse();

    LuaParser p(file, std::move(luaLexer.GetTokens()));
    p.Parse();

    LuaSyntaxTree t;
    t.BuildTree(p);

    LuaStyle style = GetStyle(uri);
    RangeFormatBuilder f(style, range);

    auto formattedText = f.GetFormatResult(t);
    range = f.GetReplaceRange();
    return formattedText;
}

std::vector<LuaTypeFormat::Result>
LuaCodeFormat::TypeFormat(const std::string &uri, std::size_t line, std::size_t character, std::string &&text,
                          ConfigMap &configMap, ConfigMap &stringTypeOptions) {
    auto file = std::make_shared<LuaFile>(std::move(text));
    LuaLexer luaLexer(file);
    luaLexer.Parse();

    LuaParser p(file, std::move(luaLexer.GetTokens()));
    p.Parse();

    LuaSyntaxTree t;
    t.BuildTree(p);

    LuaStyle style = GetStyle(uri);

    LuaTypeFormatOptions typeFormatOptions;
    LuaTypeFormat tf(typeFormatOptions);
    tf.Analyze("\n", line, character, t, style);
    return tf.GetResult();
}

std::vector<LuaDiagnosticInfo> LuaCodeFormat::Diagnostic(const std::string &uri, std::string &&text) {
    auto file = std::make_shared<LuaFile>(std::move(text));
    LuaLexer luaLexer(file);
    luaLexer.Parse();

    LuaParser p(file, std::move(luaLexer.GetTokens()));
    p.Parse();

    LuaSyntaxTree t;
    t.BuildTree(p);

    LuaStyle style = GetStyle(uri);

    LuaDiagnosticStyle diagnosticStyle;
    DiagnosticBuilder diagnosticBuilder(style, diagnosticStyle);
    diagnosticBuilder.DiagnosticAnalyze(t);
    return MakeDiagnosticInfo(diagnosticBuilder.GetDiagnosticResults(t), file);
}

std::vector<LuaDiagnosticInfo> LuaCodeFormat::SpellCheck(const std::string &uri, std::string &&text,
                                                         const CodeSpellChecker::CustomDictionary &tempDict) {
    auto file = std::make_shared<LuaFile>(std::move(text));
    LuaLexer luaLexer(file);
    luaLexer.Parse();

    LuaParser p(file, std::move(luaLexer.GetTokens()));
    p.Parse();

    LuaSyntaxTree t;
    t.BuildTree(p);

    LuaStyle style = GetStyle(uri);

    LuaDiagnosticStyle diagnosticStyle;
    DiagnosticBuilder diagnosticBuilder(style, diagnosticStyle);
    _spellChecker->SetCustomDictionary(tempDict);
    diagnosticBuilder.SetSpellChecker(_spellChecker);
    diagnosticBuilder.DiagnosticAnalyze(t);
    return MakeDiagnosticInfo(diagnosticBuilder.GetDiagnosticResults(t), file);
}

std::vector<SuggestItem> LuaCodeFormat::SpellCorrect(const std::string &word) {
    std::string letterWord = word;
    for (auto &c: letterWord) {
        c = std::tolower(c);
    }
    bool upperFirst = false;
    if (std::isupper(word.front())) {
        upperFirst = true;
    }

    auto suggests = _spellChecker->GetSuggests(letterWord);

    for (auto &suggest: suggests) {
        if (!suggest.Term.empty()) {
            if (upperFirst) {
                suggest.Term[0] = std::toupper(suggest.Term[0]);
            }
        }
    }
    return suggests;
}

LuaStyle &LuaCodeFormat::GetStyle(const std::string &uri) {
    std::shared_ptr<LuaEditorConfig> editorConfig = nullptr;
    std::size_t matchProcess = 0;
    for (auto &config: _configs) {
        if (string_util::StartWith(uri, config.Workspace)) {
            if (config.Workspace.size() > matchProcess) {
                matchProcess = config.Workspace.size();
                editorConfig = config.Editorconfig;
            }
        }
    }

    if (editorConfig) {
        return editorConfig->Generate(uri);
    }
    return _defaultStyle;
}

std::vector<LuaDiagnosticInfo> LuaCodeFormat::MakeDiagnosticInfo(const std::vector<LuaDiagnostic> &diagnostics,
                                                                 std::shared_ptr<LuaFile> file) {
    std::vector<LuaDiagnosticInfo> results;
    for (auto &diagnostic: diagnostics) {
        auto result = results.emplace_back();
        result.Type = diagnostic.Type;
        result.Message = diagnostic.Message;
        result.Data = diagnostic.Data;
        result.Start.Line = file->GetLine(diagnostic.Range.StartOffset);
        result.Start.Col = file->GetColumn(diagnostic.Range.StartOffset);
        result.End.Line = file->GetLine(diagnostic.Range.EndOffset);
        result.End.Col = file->GetColumn(diagnostic.Range.EndOffset);
    }

    return results;
}

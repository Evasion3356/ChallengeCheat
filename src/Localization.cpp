#include "Localization.h"
#include "LocalizationData.h"
#include "Config.h"
#include "Log.h"
#include "script.h" // LANGUAGE::_GET_CURRENT_LANGUAGE_ID()

#include <array>

namespace
{
	constexpr int kLanguageCount = static_cast<int>(Localization::Language::Count);
	constexpr int kMsgCount = static_cast<int>(Localization::Msg::Count);

	// One row per Language (enum order), one column per Msg. "{}" is replaced
	// with the game's own Dead Eye term (NeedDeadEyeAiming, NeedDeadEye) or
	// with this language's own "Complete" verb (AllItemsCredited).
	// LLM-assisted, unreviewed -- see Localization.h.
	constexpr std::string_view kText[kLanguageCount][kMsgCount] =
	{
		// en-US
		{ "Advance", "Complete", "Already fully completed",
		  "You must be on horseback for this one -- mount up and try again.",
		  "You must be riding a train for this one -- hop aboard and try again.",
		  "Raise your binoculars (or scope) for this one and try again.",
		  "You must be aiming a weapon with {} active for this one -- aim, activate {}, and try again.",
		  "{} must be active for this one -- activate it and try again.",
		  "Every item for this rank has already been credited -- if the rank isn't complete, use \"{}\".",
		  "No known way to progress this goal yet." },
		// fr-FR
		{ "Avancer", "Terminer", "Déjà entièrement terminé",
		  "Vous devez être à cheval pour celui-ci : montez en selle et réessayez.",
		  "Vous devez être à bord d'un train pour celui-ci : montez à bord et réessayez.",
		  "Levez vos jumelles (ou votre lunette) pour celui-ci et réessayez.",
		  "Vous devez viser avec une arme, {} activé, pour celui-ci : visez, activez {} et réessayez.",
		  "{} doit être activé pour celui-ci : activez-le et réessayez.",
		  "Tous les éléments de ce rang ont déjà été comptabilisés : si le rang n'est pas terminé, utilisez « {} ».",
		  "Aucun moyen connu de faire progresser cet objectif pour l'instant." },
		// de-DE
		{ "Voranbringen", "Abschließen", "Bereits vollständig abgeschlossen",
		  "Dafür musst du zu Pferd sein – steig auf und versuche es erneut.",
		  "Dafür musst du in einem Zug fahren – steig ein und versuche es erneut.",
		  "Nimm dafür dein Fernglas (oder Zielfernrohr) hoch und versuche es erneut.",
		  "Dafür musst du mit aktiviertem {} eine Waffe anvisieren – ziele, aktiviere {} und versuche es erneut.",
		  "{} muss dafür aktiv sein – aktiviere es und versuche es erneut.",
		  "Alle Elemente dieses Rangs wurden bereits angerechnet – ist der Rang nicht abgeschlossen, nutze „{}“.",
		  "Für dieses Ziel ist noch keine Methode bekannt." },
		// it-IT
		{ "Avanza", "Completa", "Già completata del tutto",
		  "Per questa devi essere a cavallo: monta in sella e riprova.",
		  "Per questa devi essere su un treno: sali a bordo e riprova.",
		  "Per questa alza il binocolo (o il mirino) e riprova.",
		  "Per questa devi mirare con un'arma con {} attivo: mira, attiva {} e riprova.",
		  "Per questa {} deve essere attivo: attivalo e riprova.",
		  "Tutti gli elementi di questo grado sono già stati conteggiati: se il grado non è completo, usa «{}».",
		  "Nessun metodo noto per far progredire questo obiettivo, per ora." },
		// es-ES
		{ "Avanzar", "Completar", "Ya completado por completo",
		  "Debes estar a caballo para esta: monta e inténtalo de nuevo.",
		  "Debes ir en un tren para esta: sube a bordo e inténtalo de nuevo.",
		  "Levanta los prismáticos (o la mira telescópica) para esta e inténtalo de nuevo.",
		  "Debes apuntar con un arma con {} activo para esta: apunta, activa {} e inténtalo de nuevo.",
		  "{} debe estar activo para esta: actívalo e inténtalo de nuevo.",
		  "Todos los elementos de este rango ya se han contabilizado: si el rango no está completo, usa «{}».",
		  "Aún no se conoce cómo hacer progresar este objetivo." },
		// pt-BR
		{ "Avançar", "Concluir", "Já totalmente concluído",
		  "Você precisa estar a cavalo para esta: monte e tente de novo.",
		  "Você precisa estar em um trem para esta: embarque e tente de novo.",
		  "Levante o binóculo (ou a luneta) para esta e tente de novo.",
		  "Você precisa mirar uma arma com {} ativo para esta: mire, ative {} e tente de novo.",
		  "{} precisa estar ativo para esta: ative e tente de novo.",
		  "Todos os itens deste nível já foram contabilizados: se o nível não estiver concluído, use “{}”.",
		  "Ainda não há um método conhecido para progredir neste objetivo." },
		// pl-PL
		{ "Posuń", "Ukończ", "Już w pełni ukończone",
		  "Musisz być na koniu: wsiądź na konia i spróbuj ponownie.",
		  "Musisz jechać pociągiem: wsiądź do niego i spróbuj ponownie.",
		  "Unieś lornetkę (lub celownik optyczny) i spróbuj ponownie.",
		  "Musisz celować z broni przy aktywnej umiejętności {}: wyceluj, aktywuj ją i spróbuj ponownie.",
		  "Musi być aktywna umiejętność {}: aktywuj ją i spróbuj ponownie.",
		  "Wszystkie elementy tego stopnia zostały już zaliczone: jeśli stopień nie jest ukończony, użyj „{}”.",
		  "Na razie nie znamy sposobu na postęp w tym celu." },
		// ru-RU
		{ "Продвинуть", "Завершить", "Уже полностью выполнено",
		  "Для этого нужно ехать верхом: сядьте на лошадь и повторите попытку.",
		  "Для этого нужно ехать на поезде: сядьте в поезд и повторите попытку.",
		  "Поднимите бинокль (или прицел) и повторите попытку.",
		  "Нужно целиться из оружия с включённым навыком «{}»: прицельтесь, включите его и повторите попытку.",
		  "Для этого нужно включить навык «{}»: включите его и повторите попытку.",
		  "Все пункты этого ранга уже засчитаны: если ранг не выполнен, используйте «{}».",
		  "Способ продвинуть эту цель пока неизвестен." },
		// ko-KR
		{ "진행", "완료", "이미 모두 완료되었습니다",
		  "말을 타고 있어야 합니다. 말에 올라탄 후 다시 시도하십시오.",
		  "열차에 탑승 중이어야 합니다. 열차에 탑승한 후 다시 시도하십시오.",
		  "쌍안경(또는 스코프)을 들고 다시 시도하십시오.",
		  "{}을(를) 활성화한 상태로 무기를 조준해야 합니다. 조준하고 {}을(를) 활성화한 후 다시 시도하십시오.",
		  "{}을(를) 활성화해야 합니다. 활성화한 후 다시 시도하십시오.",
		  "이 등급의 모든 항목이 이미 반영되었습니다. 등급이 완료되지 않았다면 '{}'을(를) 사용하십시오.",
		  "이 목표를 진행시키는 방법이 아직 알려지지 않았습니다." },
		// zh-TW
		{ "推進", "完成", "已全部完成",
		  "必須騎在馬上才行，請上馬後再試一次。",
		  "必須搭乘火車才行，請上車後再試一次。",
		  "請舉起雙筒望遠鏡（或瞄準鏡）後再試一次。",
		  "必須在{}啟動時瞄準武器才行，請瞄準並啟動{}後再試一次。",
		  "必須啟動{}才行，請啟動後再試一次。",
		  "此階級的所有項目都已計入，若階級尚未完成，請使用「{}」。",
		  "目前尚無已知方法推進此目標。" },
		// ja-JP
		{ "進める", "クリア", "すでにすべてクリア済みです",
		  "この項目は騎乗中でないと進行しません。馬に乗ってからもう一度お試しください。",
		  "この項目は列車に乗車中でないと進行しません。列車に乗ってからもう一度お試しください。",
		  "双眼鏡(またはスコープ)を構えてからもう一度お試しください。",
		  "{}を発動した状態で武器を構える必要があります。構えて{}を発動してから、もう一度お試しください。",
		  "{}を発動する必要があります。発動してからもう一度お試しください。",
		  "このランクの項目はすべて加算済みです。ランクが未達成の場合は「{}」を使用してください。",
		  "この目標を進める方法はまだ判明していません。" },
		// es-MX
		{ "Avanzar", "Completar", "Ya completado por completo",
		  "Debes estar a caballo para esta: monta e inténtalo de nuevo.",
		  "Debes ir en un tren para esta: sube a bordo e inténtalo de nuevo.",
		  "Levanta los binoculares (o la mira telescópica) para esta e inténtalo de nuevo.",
		  "Debes apuntar con un arma con {} activo para esta: apunta, activa {} e inténtalo de nuevo.",
		  "{} debe estar activo para esta: actívalo e inténtalo de nuevo.",
		  "Todos los elementos de este rango ya se han contabilizado: si el rango no está completo, usa «{}».",
		  "Aún no se sabe cómo hacer progresar este objetivo." },
		// zh-CN
		{ "推进", "完成", "已全部完成",
		  "必须骑在马上才行，请上马后再试一次。",
		  "必须乘坐火车才行，请上车后再试一次。",
		  "请举起双筒望远镜（或瞄准镜）后再试一次。",
		  "必须在{}开启时瞄准武器才行，请瞄准并开启{}后再试一次。",
		  "必须开启{}才行，请开启后再试一次。",
		  "此等级的所有项目均已计入，若等级尚未完成，请使用“{}”。",
		  "目前尚无已知方法推进此目标。" },
	};

	Localization::Language g_current = Localization::Language::English;
	bool g_resolved = false;

	// kText with "{}" filled in, for g_current. Rebuilt by Refresh().
	std::array<std::string, kMsgCount> g_text;

	struct LanguageCode { std::string_view code; Localization::Language language; };
	constexpr LanguageCode kCodes[] = {
		{ "en-US", Localization::Language::English },
		{ "fr-FR", Localization::Language::French },
		{ "de-DE", Localization::Language::German },
		{ "it-IT", Localization::Language::Italian },
		{ "es-ES", Localization::Language::Spanish },
		{ "pt-BR", Localization::Language::PortugueseBrazilian },
		{ "pl-PL", Localization::Language::Polish },
		{ "ru-RU", Localization::Language::Russian },
		{ "ko-KR", Localization::Language::Korean },
		{ "zh-TW", Localization::Language::ChineseTraditional },
		{ "ja-JP", Localization::Language::Japanese },
		{ "es-MX", Localization::Language::SpanishMexican },
		{ "zh-CN", Localization::Language::ChineseSimplified },
	};

	bool TryParseOverride(std::string_view code, Localization::Language& out)
	{
		for (const auto& entry : kCodes)
		{
			if (code == entry.code)
			{
				out = entry.language;
				return true;
			}
		}
		return false;
	}

	void ReplaceAll(std::string& text, std::string_view token, std::string_view value)
	{
		for (std::size_t pos = text.find(token); pos != std::string::npos; pos = text.find(token, pos + value.size()))
			text.replace(pos, token.size(), value);
	}
}

namespace Localization
{
	void Refresh()
	{
		const std::string& languageOverride = Config::Get().Language;

		Language resolved;
		if (TryParseOverride(languageOverride, resolved))
		{
			g_current = resolved;
		}
		else
		{
			// "auto" and any typo'd override both defer to the game's language.
			const std::int32_t raw = LANGUAGE::_GET_CURRENT_LANGUAGE_ID();
			g_current = (raw < 0 || raw >= kLanguageCount) ? Language::English : static_cast<Language>(raw);
		}

		const int lang = static_cast<int>(g_current);
		for (int i = 0; i < kMsgCount; i++)
		{
			const auto msg = static_cast<Msg>(i);
			std::string text(kText[lang][i]);
			if (msg == Msg::NeedDeadEyeAiming || msg == Msg::NeedDeadEye)
				ReplaceAll(text, "{}", LocalizationData::kDeadEye[lang]);
			else if (msg == Msg::AllItemsCredited)
				ReplaceAll(text, "{}", kText[lang][static_cast<int>(Msg::Complete)]);
			g_text[i] = std::move(text);
		}

		g_resolved = true;
		Log::Write("Localization::Refresh -> language index {} (ini override='{}')", static_cast<int>(g_current), languageOverride);
	}

	Language Current()
	{
		if (!g_resolved)
			Refresh();
		return g_current;
	}

	std::string_view CategoryName(int category)
	{
		if (category < 0 || category >= LocalizationData::kCategoryCount)
			return "";
		return LocalizationData::kCategoryNames[static_cast<int>(Current())][category];
	}

	std::string_view RankObjective(int category, int rank)
	{
		if (category < 0 || category >= LocalizationData::kCategoryCount)
			return {};
		rank = rank < 1 ? 1 : (rank > 10 ? 10 : rank);
		return LocalizationData::kRankObjectives[static_cast<int>(Current())][category][rank - 1];
	}

	std::string_view Text(Msg msg)
	{
		Current(); // resolves g_text on first use
		return g_text[static_cast<int>(msg)];
	}
}

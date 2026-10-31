#include "PCH.h"
#include "DiscordDock.h"
#include <QtCore/QTimer>
#include <QtWidgets/QAbstractItemView>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QStyle>
#include <QtWidgets/QToolButton>
#include <obs-module.h>
#include <windows.h>
#include "DiscordVoice.h"
#include "Messaging.h"

using namespace CharacterControlOBS;
using DiscordBear::SpeakingStatus;

namespace
{
constexpr auto DOCK_ID = "CharacterControlDiscordVoice";
// The dock reads the connection's state on a timer rather than being told, the source's thread can't touch widgets
constexpr int REFRESH_MILLISECONDS = 250;
constexpr int STATUS_DOT_SIZE = 10;
// Characters a server or channel box keeps room for, so the row fits a narrow dock. The open list shows names in full.
constexpr int COMBO_BOX_MINIMUM_CHARACTERS = 4;
constexpr auto CONNECTED_COLOUR = "#3ba55d";
constexpr auto WAITING_COLOUR = "#faa61a";
constexpr auto PROBLEM_COLOUR = "#ed4245";

QString Text(const char* Key)
{
	return QString::fromUtf8(obs_module_text(Key));
}

QString Name(const DiscordBear::NamedId& Item)
{
	return QString::fromStdString(Item.Name.empty() ? Item.Id : Item.Name);
}

// Fills a server or channel box under a placeholder for nothing picked, and selects Selected. One picked earlier that
// Discord didn't list, e.g. a server left since, is shown by its ID. False while the box's list is open, which is left
// alone so it doesn't change under the user's cursor.
bool FillComboBox(QComboBox* Box, const QString& Placeholder, const std::vector<DiscordBear::NamedId>& Items, const std::string& Selected)
{
	if(Box->view()->isVisible())
	{
		return false;
	}

	const QSignalBlocker Blocker(Box);
	Box->clear();
	Box->addItem(Placeholder, QString());
	int SelectedIndex = 0;

	for(const DiscordBear::NamedId& Item : Items)
	{
		Box->addItem(Name(Item), QString::fromStdString(Item.Id));

		if(Item.Id == Selected)
		{
			SelectedIndex = Box->count() - 1;
		}
	}

	if(SelectedIndex == 0 && !Selected.empty())
	{
		Box->addItem(QString::fromStdString(Selected), QString::fromStdString(Selected));
		SelectedIndex = Box->count() - 1;
	}

	Box->setCurrentIndex(SelectedIndex);
	// The box itself may be narrower than the names
	Box->view()->setMinimumWidth(Box->view()->sizeHintForColumn(0) + Box->style()->pixelMetric(QStyle::PM_ScrollBarExtent));
	return true;
}

QString MemberText(const VoiceRoster::MemberState& State)
{
	QString Line = QString::fromStdString(State.Member.Name.empty() ? State.Member.User : State.Member.Name);

	if(State.Speaking)
	{
		Line += Text(" - speaking");
	}

	if(State.Member.Deafened)
	{
		Line += Text(" (deafened)");
	} else if(State.Member.Muted)
	{
		Line += Text(" (muted)");
	}

	return Line;
}

const char* StatusColour(const SpeakingStatus Status)
{
	switch(Status)
	{
		case SpeakingStatus::Connected :
			return CONNECTED_COLOUR;
		case SpeakingStatus::NotSetUp :
		case SpeakingStatus::Starting :
		case SpeakingStatus::NotInVoiceChannel :
			return WAITING_COLOUR;
		case SpeakingStatus::Unavailable :
		case SpeakingStatus::DiscordNotRunning :
		case SpeakingStatus::NotAuthorized :
		case SpeakingStatus::NoTraffic :
			break;
	}

	return PROBLEM_COLOUR;
}

// One row, small enough to sit anywhere: a status dot whose tooltip says how the connection is doing and who is in the
// call, the server and voice channel to watch, and a button to fetch the lists again.
class DiscordDock final : public QWidget
{
public:
	explicit DiscordDock(QWidget* Parent) : QWidget(Parent)
	{
		StatusDot = new QLabel(this);
		StatusDot->setFixedSize(STATUS_DOT_SIZE, STATUS_DOT_SIZE);
		FollowBox = new QCheckBox(Text("Follow me"), this);
		FollowBox->setToolTip(Text("Follow me into whichever voice channel I join"));
		FollowBox->setVisible(DiscordVoice::CAN_FOLLOW_USER);
		ServerBox = MakeComboBox(Text("Discord server"));
		ChannelBox = MakeComboBox(Text("Voice channel to watch"));
		auto* RefreshButton = new QToolButton(this);
		RefreshButton->setIcon(style()->standardIcon(QStyle::SP_BrowserReload));
		RefreshButton->setToolTip(Text("Fetch the server and channel lists again"));
		AuthorizeButton = new QToolButton(this);
		AuthorizeButton->setText(Text("Authorize"));
		AuthorizeButton->setToolTip(Text("Discord turned StreamKit down. Show StreamKit to authorize it again."));

		auto* Layout = new QHBoxLayout(this);
		Layout->setContentsMargins(4, 2, 4, 2);
		Layout->setSpacing(4);
		Layout->addWidget(StatusDot);
		Layout->addWidget(FollowBox);
		Layout->addWidget(ServerBox, 1);
		Layout->addWidget(ChannelBox, 1);
		Layout->addWidget(RefreshButton);
		Layout->addWidget(AuthorizeButton);
		setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);

		connect(FollowBox, &QCheckBox::toggled, this, [](const bool Checked)
		{
			if(DiscordVoice* Voice = DiscordVoice::Get())
			{
				Voice->SetFollowUser(Checked);
			}
		});
		connect(ServerBox, &QComboBox::activated, this, [this](const int Index)
		{
			OnServerPicked(Index);
		});
		connect(ChannelBox, &QComboBox::activated, this, [this](const int Index)
		{
			OnChannelPicked(Index);
		});
		connect(RefreshButton, &QToolButton::clicked, this, [this]
		{
			RequestLists();
		});
		connect(AuthorizeButton, &QToolButton::clicked, this, []
		{
			if(const DiscordVoice* Voice = DiscordVoice::Get())
			{
				Voice->ShowSetup();
			}
		});

		auto* Timer = new QTimer(this);
		connect(Timer, &QTimer::timeout, this, [this]
		{
			Refresh();
		});
		Timer->start(REFRESH_MILLISECONDS);

		Refresh();
		RequestLists();
	}

private:
	QComboBox* MakeComboBox(const QString& ToolTip)
	{
		auto* Box = new QComboBox(this);
		Box->setToolTip(ToolTip);
		Box->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
		Box->setMinimumContentsLength(COMBO_BOX_MINIMUM_CHARACTERS);
		return Box;
	}

	void RequestLists() const
	{
		if(const DiscordVoice* Voice = DiscordVoice::Get())
		{
			Voice->RequestServers();

			if(!PickedGuild.empty())
			{
				Voice->RequestVoiceChannels(PickedGuild);
			}
		}
	}

	void OnServerPicked(const int Index)
	{
		PickedGuild = ServerBox->itemData(Index).toString().toStdString();

		if(const DiscordVoice* Voice = DiscordVoice::Get(); Voice && !PickedGuild.empty())
		{
			Voice->RequestVoiceChannels(PickedGuild);
		}

		Refresh();
	}

	void OnChannelPicked(const int Index) const
	{
		const std::string Channel = ChannelBox->itemData(Index).toString().toStdString();

		if(DiscordVoice* Voice = DiscordVoice::Get(); Voice && !Channel.empty() && !PickedGuild.empty())
		{
			Voice->WatchChannel(PickedGuild, Channel);
		}
	}

	void Refresh()
	{
		DiscordVoice* Voice = DiscordVoice::Get();
		setEnabled(Voice != nullptr);

		if(Voice == nullptr)
		{
			return;
		}

		const DiscordVoice::Settings Settings = Voice->GetSettings();

		// The server shown starts as the saved one. Picking another only saves it once a channel in it is picked.
		if(!PickedGuildLoaded)
		{
			PickedGuild = Settings.Guild;
			PickedGuildLoaded = true;
		}

		RefreshStatus(*Voice);

		{
			const QSignalBlocker Blocker(FollowBox);
			FollowBox->setChecked(Settings.FollowUser);
		}

		ServerBox->setEnabled(!Settings.FollowUser);
		ChannelBox->setEnabled(!Settings.FollowUser);

		if(const uint64_t Version = Voice->GetListsVersion(); Version != ShownListsVersion || PickedGuild != ShownChannelsGuild)
		{
			const std::optional<std::vector<DiscordBear::NamedId>> Channels = Voice->GetVoiceChannels(PickedGuild);
			const QString ChannelPlaceholder = PickedGuild.empty() || Channels ? Text("Channel") : Text("Loading...");
			const bool ServersFilled = FillComboBox(ServerBox, Text("Server"), Voice->GetServers(), PickedGuild);
			const bool ChannelsFilled = FillComboBox(ChannelBox, ChannelPlaceholder, Channels.value_or(std::vector<DiscordBear::NamedId>()),
				PickedGuild == Settings.Guild ? Settings.Channel : std::string());

			if(ServersFilled && ChannelsFilled)
			{
				ShownListsVersion = Version;
				ShownChannelsGuild = PickedGuild;
			}
		}
	}

	void RefreshStatus(const DiscordVoice& Voice)
	{
		const SpeakingStatus Status = Voice.GetStatus();
		const DiscordBear::WatchedChannel Watched = Voice.GetWatched();
		QString ToolTip = Text("Discord voice") + ": " + QString::fromUtf8(DiscordBear::GetStatusText(Status));

		if(!Watched.Channel.empty())
		{
			ToolTip += "\n" + Text("Watching") + ": " + QString::fromStdString(Watched.Name.empty() ? Watched.Channel : Watched.Name);
		}

		const std::vector<VoiceRoster::MemberState> Members = Voice.GetMembers();

		if(!Members.empty())
		{
			ToolTip += "\n\n" + Text("In the call") + ":";

			for(const VoiceRoster::MemberState& State : Members)
			{
				ToolTip += "\n" + MemberText(State);
			}
		}

		if(StatusDot->toolTip() != ToolTip)
		{
			StatusDot->setToolTip(ToolTip);
		}

		const QString DotStyle = QString("background-color: %1; border-radius: %2px;").arg(StatusColour(Status)).arg(STATUS_DOT_SIZE / 2);

		if(StatusDot->styleSheet() != DotStyle)
		{
			StatusDot->setStyleSheet(DotStyle);
		}

		AuthorizeButton->setVisible(Status == SpeakingStatus::NotAuthorized);
	}

	QLabel* StatusDot = nullptr;
	QCheckBox* FollowBox = nullptr;
	QComboBox* ServerBox = nullptr;
	QComboBox* ChannelBox = nullptr;
	QToolButton* AuthorizeButton = nullptr;

	// The server whose channels are shown
	DiscordBear::GuildId PickedGuild;
	bool PickedGuildLoaded = false;
	uint64_t ShownListsVersion = UINT64_MAX;
	DiscordBear::GuildId ShownChannelsGuild;
};

template<typename Function>
Function GetFrontendFunction(const HMODULE Frontend, const char* FunctionName)
{
	return reinterpret_cast<Function>(reinterpret_cast<void*>(GetProcAddress(Frontend, FunctionName)));
}
}

void CharacterControlOBS::AddDiscordDock()
{
	using GetMainWindowFunction = void* (*)();
	using AddDockByIdFunction = bool (*)(const char*, const char*, void*);

	// Looked up rather than linked, as the OBS build this compiles against has no frontend. OBS itself always has it loaded.
	const HMODULE Frontend = GetModuleHandleW(L"obs-frontend-api.dll");
	const auto GetMainWindow = Frontend != nullptr ? GetFrontendFunction<GetMainWindowFunction>(Frontend, "obs_frontend_get_main_window") : nullptr;
	const auto AddDockById = Frontend != nullptr ? GetFrontendFunction<AddDockByIdFunction>(Frontend, "obs_frontend_add_dock_by_id") : nullptr;

	if(GetMainWindow == nullptr || AddDockById == nullptr)
	{
		WARN_("OBS's frontend isn't loaded, so there is no Discord Voice dock.");
		return;
	}

	auto* MainWindow = static_cast<QMainWindow*>(GetMainWindow());

	if(MainWindow == nullptr)
	{
		WARN_("OBS has no main window yet, so there is no Discord Voice dock.");
		return;
	}

	// OBS takes the dock over and deletes it with the main window
	auto* Dock = new DiscordDock(MainWindow);

	if(!AddDockById(DOCK_ID, obs_module_text("Discord Voice"), Dock))
	{
		WARN_("OBS wouldn't add the Discord Voice dock.");
		delete Dock;
	}
}

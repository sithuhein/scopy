#include "iiomonitor.hpp"
#include "iiomonitorgeneralsettingsmenu.hpp"

#include "gui/generic_menu.hpp"
#include "gui/verticalcontrol.hpp"
#include "gui/customSwitch.hpp"
#include "dynamicWidget.hpp"
#include <QtConcurrent>
#include <QFuture>

using namespace adiscope;

DataLoggerTool::DataLoggerTool(struct iio_context *ctx, Filter *filt,
    ToolMenuItem *toolMenuItem,
    QJSEngine *engine, ToolLauncher *parent):
	Tool(ctx, toolMenuItem, nullptr, "DataLogger",
         parent),
    ui(new Ui::IIOMonitor),
	m_context(contextOpen(ctx, "")),
	m_timer(new QTimer(this))
{
	ui->setupUi(this);
	run_button = nullptr;

	m_colors = {QColor("#ff7200"),QColor("#9013fe"), QColor(Qt::green),QColor(Qt::cyan), QColor(Qt::magenta),
				QColor(Qt::yellow), QColor(Qt::gray), QColor(Qt::darkRed), QColor(Qt::darkGreen),
				QColor(Qt::darkBlue), QColor(Qt::darkGray),QColor(Qt::black)};

	scopy::gui::ToolViewRecipe recepie;
	recepie.helpBtnUrl = "";
	recepie.hasRunBtn = true;
	recepie.hasSingleBtn = true;
	recepie.hasPairSettingsBtn = true;
	recepie.hasPrintBtn = false;
	recepie.hasChannels = true;
	recepie.channelsPosition = scopy::gui::ChannelsPositionEnum::VERTICAL;

	m_monitorChannelManager = new scopy::gui::ChannelManager(recepie.channelsPosition);
	m_monitorChannelManager->setChannelIdVisible(false);
	m_monitorChannelManager->setToolStatus("Stopped");

	m_toolView = scopy::gui::ToolViewBuilder(recepie,m_monitorChannelManager).build();

	m_generalSettingsMenu = generateMenu("General settings", new QColor("blue"));
	m_toolView->setGeneralSettingsMenu(m_generalSettingsMenu,false);

	m_dmmList=getDmmList(m_context);


	connect(m_toolView->getRunBtn(), &QPushButton::toggled, this, [=](bool toggled){
		toggleTimer(toggled);

		//update status and if needed start data logging
		if(!m_toolView->getRunBtn()->isChecked()){
			m_monitorChannelManager->setToolStatus("Stopped");
		}else {
			if(dataLogger->isDataLoggerOn()){
				m_monitorChannelManager->setToolStatus("Data Logging");
				if(!activeChannels.empty()){
					for(auto ch : activeChannels.keys()){
						QString name = QString::fromStdString(activeChannels[ch].dmm->getName() + ":" + activeChannels[ch].dmmId);
						dataLogger->createChannel(name, adiscope::Type::DOUBLE);
					}
				}
				dataLogger->startLogger();
			}else{
				m_monitorChannelManager->setToolStatus("Running");
			}
		}
	});

	connect(m_toolView->getRunBtn(), SIGNAL(toggled(bool)),runButton(), SLOT(setChecked(bool)));
	connect(runButton(), SIGNAL(toggled(bool)), m_toolView->getRunBtn(),SLOT(setChecked(bool)));

	connect(m_toolView->getSingleBtn(), &QPushButton::toggled,this, [=](){
		readChannelValues();
	});

	connect(m_toolView->getSingleBtn(), &QPushButton::clicked,this, [=](bool checked){
		m_toolView->getSingleBtn()->setChecked(!checked);
		m_toolView->getRunBtn()->toggled(false);
	});

	initMonitorToolView();
	m_toolView->addFixedCentralWidget(m_customColGrid,0,0,0,0);

	connect(m_timer, &QTimer::timeout , this,&DataLoggerTool::readChannelValues);

	connect(this, &DataLoggerTool::RecordingIntervalChanged, this , [=](double interval){
		VALUE_READING_TIME_INTERVAL = interval;
		m_timer->setInterval(interval);
	});
}

void DataLoggerTool::initMonitorToolView(){

	int chId = 1;
	m_customColGrid = new CustomColQGridLayout(100,this);

	for(libm2k::analog::DMM* dmm : m_dmmList){
		std::vector<ChannelWidget*> channelList;

		scopy::gui::DataLoggerToolGenericMenu *menu = new scopy::gui::DataLoggerToolGenericMenu(this);
		menu->init(QString::fromStdString(dmm->getName()),new QColor("green"),false);

		ChannelWidget *mainCh_widget =
				m_toolView->buildNewChannel(m_monitorChannelManager, menu, true, chId, false, false, QColor("green"), QString::fromStdString(dmm->getName()), QString::fromStdString(dmm->getName()) );

//		mainCh_widget->enableButton()->setChecked(false);
		chId++;
		for(auto channel : dmm->readAll()){

			QColor channelColor = getChannelColor(chId);

			scopy::gui::DataLoggerToolGenericMenu *channelMenu = new scopy::gui::DataLoggerToolGenericMenu(this);
			channelMenu->init(QString::fromStdString(dmm->getName() + ": " + channel.id),new QColor(channelColor),false);

			ChannelWidget *ch_widget =
					m_toolView->buildNewChannel(m_monitorChannelManager, channelMenu , true, chId, false, false, channelColor, QString::fromStdString(channel.id), QString::fromStdString(channel.id) );

			channelList.push_back(ch_widget);

			ch_widget->enableButton()->setChecked(false);
//			ch_widget->hide();


			adiscope::ChannelMonitorComponent* monitor = new adiscope::ChannelMonitorComponent();
			monitor->setID(chId);
			monitor->init(0,QString::fromStdString(channel.unit_name),QString::fromStdString(channel.unit_symbol),QString::fromStdString(dmm->getName() + ": " + channel.id),channelColor );
			monitor->setChannelId(channel.id);
			monitor->setRecordingInterval(VALUE_READING_TIME_INTERVAL/1000);
			monitor->setHistoryDuration(10);

			//connect widget to group settings (main menu)
			createConnections(menu,monitor);
			connect(this, &DataLoggerTool::toggleAll, this, [=](bool toggled){
				if(ch_widget->enableButton()->isChecked() != toggled){
					ch_widget->enableButton()->click();
				}
			});

			//connect menu switches to widget
			createConnections(channelMenu,monitor);

			//add connections from general settings
			connect(this, &DataLoggerTool::PrecisionChanged, this, [=](int precision){
				monitor->updateLcdNumberPrecision(precision);
			});

			connect(m_toolView->getRunBtn(), &QPushButton::toggled,this, [=](bool toggled){
				if(dataLogger->isDataLoggerOn()){
					ch_widget->enableButton()->setDisabled(toggled);
				}
			});

			int widgetId =m_customColGrid->addQWidgetToList(monitor);

			// logic for enable/disable channels (monitors)
			connect(ch_widget, &ChannelWidget::enabled,this, [=](bool enabled){
				if (enabled) {
					m_customColGrid->addWidget(widgetId);
					activeChannels[chId].dmmId = channel.id;
					activeChannels[chId].dmm = dmm;
					activeChannels[chId].numberOfTabsUsing++; //increment number of tabs using this channel
					m_activeMonitors[chId] = monitor;

				} else {
					//decrese number of tabs using this channel if no channel left remove from active list
					activeChannels[chId].numberOfTabsUsing--;
					if (activeChannels[chId].numberOfTabsUsing == 0) {
						activeChannels.erase(activeChannels.find(chId));
					}
					m_activeMonitors.erase(m_activeMonitors.find(chId));
					m_customColGrid->removeWidget(widgetId);
				}
			});
			ch_widget->enableButton()->click();
			chId++;
		}
		m_toolView->buildChannelGroup(m_monitorChannelManager, mainCh_widget,channelList);
	}
}

scopy::gui::ToolView* DataLoggerTool::getToolView(){
    return m_toolView;
}

std::vector<libm2k::analog::DMM*> DataLoggerTool::getDmmList(libm2k::context::Context* context){
	return context->getAllDmm() ;
}

void DataLoggerTool::readChannelValues(){

	QMap<int,double> channelUpdatedValue;

	if(!activeChannels.empty()){
		for(auto ch : activeChannels.keys()){
			QFuture<libm2k::analog::DMM_READING> updatedRead = QtConcurrent::run(this,&DataLoggerTool::readChVal,ch);
			//handle monitors value update
			auto test = updatedRead.result();
			std::cout << test.id;
			if(m_activeMonitors.contains(ch)){
				m_activeMonitors[ch]->updateValue(updatedRead.result().value,QString::fromStdString(updatedRead.result().unit_name), QString::fromStdString(updatedRead.result().unit_symbol));
			}
			channelUpdatedValue[ch] = updatedRead.result().value;
			///emit data logging ch val update
			if(dataLogger->isDataLoggerOn()){
				Q_EMIT updateValue(QString::fromStdString(activeChannels[ch].dmm->getName() + ":" + activeChannels[ch].dmmId),QString::number(updatedRead.result().value));
			}
		}
	}
}

libm2k::analog::DMM_READING DataLoggerTool::readChVal(int ch){
	return  activeChannels[ch].dmm->readChannel(activeChannels[ch].dmmId);
}

void DataLoggerTool::toggleTimer(bool enabled){
    if(enabled){
		m_timer->start(VALUE_READING_TIME_INTERVAL);
    }else{
        m_timer->stop();
    }
}

void DataLoggerTool::createConnections(scopy::gui::DataLoggerToolGenericMenu* menu,adiscope::ChannelMonitorComponent* monitor){
	connect(menu,&scopy::gui::DataLoggerToolGenericMenu::togglePeakHolder,this,[=](bool toggled){
	   monitor->displayPeakHold(toggled);
	});

	connect(menu, &scopy::gui::DataLoggerToolGenericMenu::toggleHistory,this,[=](bool toggled){
		monitor->displayHistory(toggled);
	});

	connect(menu, &scopy::gui::DataLoggerToolGenericMenu::toggleScale,this, [=](bool toggled){
	   monitor->displayScale(toggled);
	});

	connect(menu, &scopy::gui::DataLoggerToolGenericMenu::resetPeakHolder, this, [=](){
	   monitor->resetPeakHolder();
	});

	connect(menu, &scopy::gui::DataLoggerToolGenericMenu::monitorColorChanged,this, [=](QString color){
		monitor->setMonitorColor(color);
	});

	connect(menu, &scopy::gui::DataLoggerToolGenericMenu::changeHistorySize, this, [=](double duration){
		monitor->setHistoryDuration(duration);
	});

	connect(menu, &scopy::gui::DataLoggerToolGenericMenu::lineStyleChanged, this, [=](Qt::PenStyle style){
		monitor->setLineStyle(style);
	});
}

QColor DataLoggerTool::generateColor(){

	if(m_color.size() < m_colors.size()){
		return m_colors.at(m_color.size());
	}
	int red = rand() % 256;
	int blue = rand() % 256;
	int green = rand() % 256;
	return QColor(red,green,blue);
}

QColor DataLoggerTool::getChannelColor(int chId){
	if(m_color.contains(chId)){
		return m_color[chId];
	}else{
		if(m_color.isEmpty()){
			m_color[chId]= generateColor();
			return m_color[chId];
		}
		QColor color;
		bool colorExists = true;
		while(colorExists){
			color = generateColor();
			for(auto it = m_color.begin(); it != m_color.end(); it++){
				if(it.value() != color){
					colorExists = false;
					break;
				}
			}
		}
		m_color[chId]=color;
		return color;
	}
}


scopy::gui::GenericMenu* DataLoggerTool::generateMenu(QString title, QColor* color){
	scopy::gui::GenericMenu *menu = new scopy::gui::GenericMenu(this);
	menu->initInteractiveMenu();
	menu->setMenuHeader(title,color,false);

	auto *showAllSection = new scopy::gui::SubsectionSeparator("Show all", false, this);

	QWidget *showAllWidget = new QWidget(this);
	auto *showAllLayout = new QHBoxLayout(showAllWidget);
	auto *showAllSWitch = new CustomSwitch(showAllWidget);
	showAllSWitch->setChecked(true);

	showAllLayout->addWidget(new QLabel("Show all",showAllWidget));
	showAllLayout->addWidget(showAllSWitch);

	showAllSection->setContent(showAllWidget);

	connect(showAllSWitch, &CustomSwitch::toggled, this, [=](bool toggled){
		Q_EMIT DataLoggerTool::toggleAll(toggled);
	});

	auto *precisionSection = new scopy::gui::SubsectionSeparator("Precision", false,this);

	QWidget *precisionWidget = new QWidget(this);
	auto *precisionLayout = new QHBoxLayout(precisionWidget);
	precisionLayout->setAlignment(precisionWidget,Qt::AlignLeft);
	auto *precisionBtn = new QPushButton("Set",precisionWidget);
	precisionBtn->setMinimumWidth(50);
	precisionBtn->setStyleSheet("QPushButton{"
										"height:25px;"
										"background-color: #4A64FF;"
										"border-radius: 4px;"
										"font-size: 12px;"
										"line-height: 14px;"
										"color: #FFFFFF;}"
										"QPushButton:hover{"
										"background-color: #4a34ff;"
										"}");

	auto *precisionValue = new QLineEdit(precisionWidget);
	precisionValue->setText("3");
	precisionLayout->addWidget(precisionBtn);
	precisionLayout->addItem(new QSpacerItem(1,1,QSizePolicy::Expanding,QSizePolicy::Fixed));
	precisionLayout->addWidget(precisionValue);

	connect(precisionBtn, &QPushButton::clicked,this, [=](){
		Q_EMIT PrecisionChanged(precisionValue->text().toInt());
	});

	precisionSection->setContent(precisionWidget);

	auto *recordingIntervalSection = new scopy::gui::SubsectionSeparator("Recording interval", false, this);
	auto recordingIntervalWidget = new QWidget(this);
	auto *recordingIntevlaLayout = new QVBoxLayout(recordingIntervalWidget);

	PositionSpinButton *recording_timer = new PositionSpinButton({
		{"s", 1},
		{"min", 60},
		{"h", 3600}
	}, tr("Timer"), 0, 3600,
	true, false, this);

	recording_timer->setValue(1);
	recordingIntevlaLayout->addWidget(recording_timer);
	connect(recording_timer, &PositionSpinButton::valueChanged, this, [=](){
		Q_EMIT RecordingIntervalChanged(recording_timer->value() * 1000);
	});

	recordingIntervalSection->setContent(recordingIntervalWidget);

	auto dataLoggingSection = new scopy::gui::SubsectionSeparator("Data Logging",true,this);

	dataLogger = new DataLogger(true,true,false);
	dataLoggingSection->setContent(dataLogger->getWidget());

	connect(this, &DataLoggerTool::updateValue, this , [=](QString name, QString value){
		dataLogger->receiveValue(name,value);
	});

	showAllSection->getButton()->click();
	precisionSection->getButton()->click();
	recordingIntervalSection->getButton()->click();
	dataLoggingSection->getButton()->click();

	menu->insertSection(showAllSection);
	menu->insertSection(precisionSection);
	menu->insertSection(recordingIntervalSection);
	menu->insertSection(dataLoggingSection);

	return menu;
}

DataLoggerTool::~DataLoggerTool()
{
	if (m_timer) { delete m_timer; }
	if (m_toolView) { delete m_toolView; }
	if (m_monitorToolView) { delete m_monitorToolView; }

    delete ui;
}

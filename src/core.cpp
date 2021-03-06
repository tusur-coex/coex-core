
#include "core.h"
#include "taskrunner.h"
#include <iostream>
#include <QFile>
#include <QStringList>
#include <QDir>
#include <QVector>
#include <QThreadPool>

Core::Core(){
	m_nMaxThreads = QThread::idealThreadCount();
};

// ---------------------------------------------------------------------

QString Core::pathToPluginsSystem(){
	QString path = "plugins";
	
	#ifdef Q_OS_LINUX
		path = "/usr/lib";
	#elif defined(Q_OS_IOS)
		path = "/usr/lib";
	#elif defined(Q_OS_WIN32)
		path = "/C/Program Files/forensictool/plugins/"; // TODO: check it
	#endif
	
	return path;
}

// ---------------------------------------------------------------------

QString Core::filter(){
	QString filter = "libforensictool-plugin-*";
	
	#ifdef Q_OS_LINUX
		filter = "libforensictool-plugin-*.so." + QString::number(VERSION_MAJOR) + "." + QString::number(VERSION_MINOR) + ".*";
	#elif defined(Q_OS_IOS)
		filter = "libforensictool-plugin-*.so." + QString::number(VERSION_MAJOR) + "." + QString::number(VERSION_MINOR) + ".*";
	#elif defined(Q_OS_WIN32)
		filter = "libforensictool-plugin-*-" + QString::number(VERSION_MAJOR) + "." + QString::number(VERSION_MINOR) + "-*.dll";
	#endif
	
	return filter;
}

// ---------------------------------------------------------------------

void Core::loadPluginsByPath(const QString &fullpathToFolder){
	QString filter = this->filter();
	QDir dirPlugins(fullpathToFolder);
	QStringList files;
	// std::cout << "Search plugins by filter: " << filter.toStdString() << "\n";
	files = dirPlugins.entryList(QStringList(filter), QDir::Files);
	for (int i = 0; i < files.size(); i++) {
		QString absolutePluginPath = dirPlugins.absolutePath() + "/" + files.at(i);
		this->loadPlugin(absolutePluginPath);
	}
}

// ---------------------------------------------------------------------

void Core::loadPluginsFromSystem(){
	this->loadPluginsByPath(this->pathToPluginsSystem());
}

// ---------------------------------------------------------------------

bool Core::loadPlugin(const QString &fullpathToPlugin){
	// std::cout << " --> Plugin '" << fullpathToPlugin.toStdString() << "' ... \n";
	QLibrary *plugin = new QLibrary(fullpathToPlugin);
	bool bIsPlugin = false;
	
	typedef forensictool::IDetectorOperationSystem* (*funcCreateDetectorOperationSystem) ();
    typedef forensictool::ITask* (*funcCreateTask) ();
    
	// try load detect operation system
	funcCreateDetectorOperationSystem createDetector = (funcCreateDetectorOperationSystem)(plugin->resolve("createDetectorOperationSystem"));
	if(createDetector)
	{
		bIsPlugin = true;
		forensictool::IDetectorOperationSystem* detect = createDetector();
		// std::cout << "OK \n ----> Found detector '" << detect->name().toStdString() << "' by '" << detect->author().toStdString() << "' ";
		m_vDetectors.push_back(detect);
	}
	
	// try load thread task
	funcCreateTask createTask = (funcCreateTask)(plugin->resolve("createTask"));
	if(createTask)
	{
		bIsPlugin = true;
		forensictool::ITask* task = createTask();
		// std::cout << "OK \n ----> Found threadTask '" << task->name().toStdString() << "' by '" << task->author().toStdString() << "' ";
		m_vTasks.push_back(task);
	}

	if (bIsPlugin) {
		m_vLibraries.push_back(plugin);
	} else {
		std::cout << "NOTHING";
		plugin->unload();
	}
	return bIsPlugin;
}

// ---------------------------------------------------------------------

QVector<forensictool::ITask *> &Core::tasks(){
	return m_vTasks;
}

// ---------------------------------------------------------------------

QVector<forensictool::IDetectorOperationSystem *> &Core::detectors(){
	return m_vDetectors;
}

// ---------------------------------------------------------------------

void Core::setMaxThreads(int nMaxThreads){
	m_nMaxThreads = nMaxThreads;
}

// ---------------------------------------------------------------------

void Core::run(forensictool::IConfig *pConfig){

	// config->
	// detect operation system
    std::cout << " > Detectiong operation system . . . \n";
	
	forensictool::ITypeOperationSystem* typeOS = NULL;
	for (int i = 0; i < m_vDetectors.size(); i++) {
		forensictool::ITypeOperationSystem* tmpTypeOS = m_vDetectors[i]->detect(pConfig->inputFolder());
		if (tmpTypeOS != NULL && typeOS != NULL) {
			std::cerr << "ERROR: found ambiguity\n";
			return;
		}
		typeOS = tmpTypeOS;
	}

	if(typeOS == NULL){
		std::cerr << "ERROR: Could not detect system\n";
		return;
	}
	
	pConfig->setTypeOS(typeOS);

	QThreadPool::globalInstance()->setMaxThreadCount(m_nMaxThreads);
	std::cout << QThreadPool::globalInstance()->maxThreadCount() << "\n";
	QVector<TaskRunner *> runners;
	for (int i = 0; i < m_vTasks.size(); i++) {
		if (m_vTasks[i]->isSupportOS(pConfig->typeOS())) {
			m_vTasks[i]->init(pConfig);
			TaskRunner *pTaskRunner = new TaskRunner(m_vTasks[i]);
			runners.push_back(pTaskRunner);
			QThreadPool::globalInstance()->start(pTaskRunner);
		}
	}
    QThreadPool::globalInstance()->waitForDone();
    std::cout << "Done";
}

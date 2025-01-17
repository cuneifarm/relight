#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "graphics_view_zoom.h"
#include "rtiexport.h"
#include "../src/imageset.h"
#include "helpdialog.h"
#include "focaldialog.h"
#include "scripts.h"
#include "queuewindow.h"
#include "httpserver.h"
#include "settingsdialog.h"

#include <QInputDialog>
#include <QFileDialog>
#include <QMessageBox>
#include <QGraphicsPixmapItem>
#include <QMouseEvent>
#include <QSettings>
#include <QListView>
#include <QStandardItemModel>
#include <QItemSelectionModel>

#include <QtConcurrent/QtConcurrent>

#include <set>
#include <iostream>

#include <assert.h>
using namespace std;

MainWindow::MainWindow(QWidget *parent) :
	QMainWindow(parent),
	ui(new Ui::MainWindow)
{
	settings = new QSettings;

	ui->setupUi(this);
	connect(ui->actionNew,        SIGNAL(triggered(bool)),  this, SLOT(newProject()));
	connect(ui->actionOpen,       SIGNAL(triggered(bool)),  this, SLOT(openProject()));

	connect(ui->actionSave,       SIGNAL(triggered(bool)),  this, SLOT(saveProject()));
	connect(ui->actionSave_as,    SIGNAL(triggered(bool)),  this, SLOT(saveProjectAs()));
	connect(ui->actionPreferences,    SIGNAL(triggered(bool)),  this, SLOT(preferences()));
	connect(ui->actionExit,       SIGNAL(triggered(bool)),  this, SLOT(quit()));

	connect(ui->actionRuler,      SIGNAL(triggered(bool)),  this, SLOT(startMeasure()));
	connect(ui->actionPrevious,   SIGNAL(triggered(bool)),  this, SLOT(previous()));
	connect(ui->actionNext,       SIGNAL(triggered(bool)),  this, SLOT(next()));
	connect(ui->actionToggle_max_luma, SIGNAL(triggered(bool)), this, SLOT(toggleMaxLuma()));
	connect(ui->actionExport_RTI, SIGNAL(triggered(bool)),  this, SLOT(exportRTI()));
	connect(ui->actionView_RTI, SIGNAL(triggered(bool)),  this, SLOT(viewRTI()));

	connect(ui->actionShow_queue, SIGNAL(triggered(bool)),  this, SLOT(showQueue()));


	connect(ui->addSphere,        SIGNAL(clicked(bool)),   this, SLOT(addSphere()));
	connect(ui->removeSphere,     SIGNAL(clicked(bool)),   this, SLOT(removeSphere()));
	connect(ui->saveLP,           SIGNAL(clicked(bool)),   this, SLOT(saveLPs()));

	connect(ui->process,          SIGNAL(clicked(bool)),   this, SLOT(process()));
	connect(ui->actionSave_LP,    SIGNAL(triggered(bool)), this, SLOT(saveLPs()));
	connect(ui->actionLoad_LP,    SIGNAL(triggered(bool)), this, SLOT(loadLP()));
	connect(ui->showSpheres,      SIGNAL(clicked(bool)),   this, SLOT(showSpheres(bool)));

	connect(ui->actionLens_parameters, SIGNAL(triggered(bool)), this, SLOT(editLensParameters()));

	connect(ui->actionHelp,       SIGNAL(triggered(bool)), this, SLOT(showHelp()));


	ui->imageList->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(ui->imageList, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(imagesContextMenu(QPoint)));

	ui->sphereList->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(ui->sphereList, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(spheresContextMenu(QPoint)));


	//connect(ui->actionProcess,  SIGNAL(triggered(bool)),            this, SLOT(process()));
	connect(ui->actionDelete_selected,     SIGNAL(triggered(bool)),   this, SLOT(deleteSelected()));

	connect(ui->imageList,     SIGNAL(currentItemChanged(QListWidgetItem *, QListWidgetItem *)), this, SLOT(openImage(QListWidgetItem *)));
	connect(ui->sphereList,     SIGNAL(currentItemChanged(QListWidgetItem *, QListWidgetItem *)), this, SLOT(changeSphere(QListWidgetItem *, QListWidgetItem *)));


	scene = new RTIScene(this);
	connect(scene, SIGNAL(borderPointMoved()), this, SLOT(updateBorderPoints()));
	connect(scene, SIGNAL(highlightMoved()), this, SLOT(updateHighlight()));

	//	connect(scene, SIGNAL(changed(QList<QRectF>)), this, SLOT(updateBalls()));


	ui->graphicsView->setScene(scene);
	ui->graphicsView->setDragMode(QGraphicsView::ScrollHandDrag);
	ui->graphicsView->setInteractive(true);
	QApplication::setOverrideCursor( Qt::ArrowCursor );

	auto *gvz = new Graphics_view_zoom(ui->graphicsView);
	connect(gvz, SIGNAL(dblClicked(QPoint)), this, SLOT(pointPicked(QPoint)));
	connect(gvz, SIGNAL(clicked(QPoint)), this, SLOT(pointClick(QPoint)));
	connect(ui->actionZoom_in,  SIGNAL(triggered(bool)), gvz, SLOT(zoomIn()));
	connect(ui->actionZoom_out, SIGNAL(triggered(bool)), gvz, SLOT(zoomOut()));


	rtiexport = new RtiExport(this);
	help = new HelpDialog(this);
	imageModel = new QStandardItemModel(ui->imageList1);
	ui->imageList1->setModel(imageModel);


	// Register model item  changed signal
	QItemSelectionModel *selectionModel = ui->imageList1->selectionModel();

	//TODO remove QListWidget!
	ui->imageList->setVisible(false);
	connect(selectionModel, SIGNAL(currentChanged(const QModelIndex &, const QModelIndex &)),
			this, SLOT(openImage(const QModelIndex &)));
	connect( imageModel , SIGNAL(itemChanged(QStandardItem *)), this, SLOT( imageChecked(QStandardItem * )));

}

MainWindow::~MainWindow() {	delete ui; }

void MainWindow::preferences() {
	if(!settings_dialog) {
		settings_dialog = new SettingsDialog();
		settings_dialog->setModal(true);
	}
	settings_dialog->show();
}

void MainWindow::clear() {
	if(imagePixmap) {
		delete imagePixmap;
		imagePixmap = nullptr;
	}
	project_filename = QString();
	if(imagePixmap)
		delete imagePixmap;
	ui->imageList->clear();
	ui->graphicsView->resetMatrix();
	ui->sphereList->clear();
	project.clear();
}



void MainWindow::newProject() {
	QString lastDir = settings->value("LastDir", QDir::homePath()).toString();
	QString dir = QFileDialog::getExistingDirectory(this, "Choose picture folder", lastDir);
	if(dir.isNull()) return;

	clear();

	project.setDir(QDir(dir));
	bool ok = project.scanDir();
	if(!project.size()) {
		QMessageBox::critical(this, "Houston we have a problem!", "Could not find images in directory: " + project.dir.path());
		return;
	}
	if(!ok) {
		//check if we can rotate a few images.
		bool canrotate = false;
		for(Image &image: project.images1) {
			if(image.size == project.imgsize)
				continue;

			if(image.isRotated(project.imgsize))
				canrotate = true;
		}
		if(canrotate) {
			int answer = QMessageBox::question(this, "Some images are rotated.", "Do you wish to uniform image rotation?", QMessageBox::Yes, QMessageBox::No);
			if(answer != QMessageBox::No)
				project.rotateImages();
		} else
			QMessageBox::critical(this, "Resolution problem", "Not all of the images in the folder have the same resolution (marked in red)");
	}

	QStringList img_ext;
	img_ext << "*.lp";
	QStringList lps = QDir(dir).entryList(img_ext);
	if(lps.size() > 1) {
	} else if(lps.size() == 1) {
		int answer = QMessageBox::question(this, "Found an .lp file", "Do you wish to load the .lp file?", QMessageBox::Yes, QMessageBox::No);
		if(answer != QMessageBox::No)
			loadLP(lps[0]);
	}

	enableActions();
	init();
}

void MainWindow::openProject() {
	QString lastDir = settings->value("LastDir", QDir::homePath()).toString();

	QString filename = QFileDialog::getOpenFileName(this, "Select a project", lastDir, "*.relight");
	if(filename.isNull())
		return;

	clear();

	project.clear();
	try {
		project.load(filename);
	} catch(QString e) {
		QMessageBox::critical(this, "Could not load the project: " + filename, "Error: " + e);
		return;
	}
	project_filename = project.dir.relativeFilePath(filename);
	enableActions();
	setupSpheres();
	setupMeasures();
	init();
}

void MainWindow::saveProject() {
	QString lastDir = settings->value("LastDir", QDir::homePath()).toString();

	if(project_filename.isNull()) {
		QString filename = QFileDialog::getSaveFileName(this, "Save file: ", lastDir, "*.relight");
		if(!filename.endsWith((".relight")))
			filename += ".relight";
		project_filename = filename;
	}
	if(!project_filename.isNull())
		project.save(project_filename);
	//TODO set window title as project filename filename
}

void MainWindow::saveProjectAs() {
	QString lastDir = settings->value("LastDir", QDir::homePath()).toString();

	QString new_filename = QFileDialog::getSaveFileName(this, "Save file: ", lastDir, "*.relight");
	if(new_filename.isNull())
		return;
	project_filename = new_filename;
	saveProject();
}

void MainWindow::enableActions() {
	ui->actionSave->setEnabled(true);
	ui->actionSave_as->setEnabled(true);
	ui->actionPrevious->setEnabled(true);
	ui->actionNext->setEnabled(true);
	ui->actionExport_RTI->setEnabled(true);
	ui->actionLoad_LP->setEnabled(true);
	ui->actionSave_LP->setEnabled(true);

	ui->addSphere->setEnabled(true);
	ui->removeSphere->setEnabled(true);
	ui->showSpheres->setEnabled(true);
	if(project.hasDirections())
		ui->actionSave_LP->setEnabled(true);
}

bool MainWindow::init() {

	if(imagePixmap)
		delete imagePixmap;
	settings->setValue("LastDir", project.dir.path());

	ui->imageList->clear();

	//create the items (name and TODO thumbnail
	int count = 0;
	imageModel->clear();
	for(Image &a: project.images1) {
		auto *item = new QListWidgetItem(QString("%1 - %2").arg(count+1).arg(a.filename), ui->imageList);
		item ->setData(Qt::UserRole, count);

		QStandardItem *poListItem = new QStandardItem;
		//if(!a.valid)
		//	poListItem->setBackground(Qt::red);
		poListItem->setText(QString("%1 - %2").arg(count+1).arg(a.filename));
		poListItem->setCheckable(true);
		// Uncheck the item
		poListItem->setCheckState(a.valid ? Qt::Checked : Qt::Unchecked);
		poListItem->setData(a.valid ? Qt::Checked : Qt::Unchecked, Qt::CheckStateRole);
		poListItem->setData(count, Qt::UserRole+1);
		imageModel->setItem(count, poListItem);

		count++;
	}

	openImage(ui->imageList->item(0), true);
	//TODO: in background load and process the images

//	addSphere();

	ui->addSphere->setEnabled(true);
	ui->removeSphere->setEnabled(true);
	ui->process->setEnabled(true);
	ui->showSpheres->setEnabled(true);
	ui->saveLP->setEnabled(true);
	return true;
}

void MainWindow::imageChecked(QStandardItem *item) {
	QModelIndex index = imageModel->indexFromItem(item);
	Image &image = project.images1[index.row()];
	bool skip = !index.data(Qt::CheckStateRole).toBool();
	if(!skip && !image.valid) {
		QMessageBox::critical(this, "Can't include this image.", "This image has a different resolution or focal, cannot include in the processing");
		item->setCheckState(Qt::Unchecked);
		return;
	}
	image.skip = skip;
//item->setBackground(image.skip? Qt::red : Qt::white);
}

void MainWindow::openImage(const QModelIndex &index) {
	// Set selection
	openImage(index.row(), false);
}

void MainWindow::openImage(QListWidgetItem *item, bool fit) {
	if(!item)
		return;
	ui->imageList->setCurrentItem(item);
	int id = item->data(Qt::UserRole).toInt();
	openImage(id, fit);
}

void MainWindow::openImage(int id, bool fit) {
	QString filename = project.images1[id].filename;//item->text();

	QImage img(project.dir.filePath(filename));
	if(img.isNull()) {
		QMessageBox::critical(this, "Houston we have a problem!", "Could not load image " + filename);
		return;
	}
	currentImage = id;

	maxLuming = false;

	size_t n = size_t(currentImage);
	if(imagePixmap)
		delete imagePixmap;
	imagePixmap = new QGraphicsPixmapItem(QPixmap::fromImage(img));
	imagePixmap->setZValue(-1);
	scene->addItem(imagePixmap);
	//if(!project.imgsize.isValid())
	//	project.imgsize = img.size();

	if(fit) {
		//find smallest problems
		double sx =  double(ui->graphicsView->width()) / project.imgsize.width();
		double sy = double(ui->graphicsView->height()) / project.imgsize.height();
		double s = std::min(sx, sy);
		ui->graphicsView->scale(s, s);
	}
	showHighlights(n);
}

void MainWindow::showHighlights(size_t n) {
	ignore_scene_changes = true;
	for(auto it: project.balls) {
		Ball *ball = it.second;
		if(!ball->fitted)
			continue;
		if(!ball->highlight)
			throw QString("Highlight should be already here!");

		QRectF mark(- QPointF(4, 4), QPointF(4, 4));
		ball->highlight->setRect(mark);
		ball->highlight->setVisible(true);

		if(!ball->lights[n].isNull()) {
			ball->highlight->setPos(ball->lights[n]);
			ball->highlight->setBrush(Qt::green);
		} else {
			ball->highlight->setPos(ball->inner.center());
			ball->highlight->setBrush(Qt::red);
		}
	}
	ignore_scene_changes = false;
}

	
bool MainWindow::lumaCallback(std::string s, int n) {
	QString str(s.c_str());
	emit lumaProgressText(str);
	emit lumaProgress(n);
	if(lumaCancelling)
		return false;
	return true;
}

void MainWindow::lumaCancel() {
	lumaCancelling = true;
}

void MainWindow::lumaFinish() {
	if(progress == nullptr)
		return;
	progress->close();
	delete progress;
	progress = nullptr;
	toggleMaxLuma();
}

void MainWindow::computeMaxLuma() {
	
	lumaCancelling = false;
	QFuture<void> future = QtConcurrent::run([this]() {
		ImageSet imageset;
		for(auto image: project.images1)
			imageset.images.push_back(image.filename);
		imageset.initImages(this->project.dir.path().toStdString().c_str());
		std::function<bool(std::string s, int n)> callback = [this](std::string s, int n)->bool { return this->lumaCallback(s, n); };
		this->maxLuma = imageset.maxImage(&callback); 
	} );
	watcher.setFuture(future);
	connect(&watcher, SIGNAL(finished()), this, SLOT(lumaFinish()));

	progress = new QProgressDialog("Building max luma image", "Cancel", 0, 100, this);
	progress->setAutoClose(false);
	progress->setAutoReset(false);
	progress->setWindowModality(Qt::WindowModal);
	connect(progress, SIGNAL(canceled()), this, SLOT(lumaCancel()));
	connect(this, SIGNAL(lumaProgress(int)), progress, SLOT(setValue(int)));
	connect(this, SIGNAL(lumaProgressText(const QString &)), progress, SLOT(setLabelText(const QString &)));
	progress->show();
}

void MainWindow::toggleMaxLuma() {
	
	if(maxLuma.isNull()) {
		computeMaxLuma();
		return;
	}
	if(maxLuming)
		openImage(ui->imageList->item(currentImage));
	else {
		if(imagePixmap)
			delete imagePixmap;
		imagePixmap = new QGraphicsPixmapItem(QPixmap::fromImage(maxLuma));
		imagePixmap->setZValue(-1);
		scene->addItem(imagePixmap);
	}
	maxLuming = !maxLuming;
}

void MainWindow::showSpheres(bool show) {
	for(auto it: project.balls) {
		Ball *ball = it.second;
		if(!ball->sphere)
			continue;
		if(show)
			scene->addItem(ball->sphere);
		else
			scene->removeItem(ball->sphere);
	}
}

void MainWindow::previous() {
	if(currentImage == 0)
		return;
	openImage(ui->imageList->item(currentImage-1));

}


void MainWindow::next() {
	if(currentImage == int(project.size()-1))
		return;
	openImage(ui->imageList->item(currentImage+1));
}

void MainWindow::startMeasure() {
	measure = new Measure();
	measure->setScene(scene);
	QApplication::setOverrideCursor( Qt::CrossCursor );
}
void MainWindow::endMeasure() {
	QApplication::setOverrideCursor( Qt::ArrowCursor );

	bool ok = true;
	double length = QInputDialog::getDouble(this, "Enter a measurement", "The distance between the two points in mm.", 0.0, 0.0, 1000000.0, 1, &ok);
	if(!ok) {
		delete measure;
		return;
	}

	measure->setLength(length);
	project.measures.push_back(measure);
	measure = nullptr;
}

void MainWindow::pointClick(QPoint p) {
	if(!measure)
		return;

	QPointF pos = ui->graphicsView->mapToScene(p);
	
	if(measure->measuring == Measure::FIRST_POINT) {
		measure->setFirstPoint(pos);

	} else if(measure->measuring == Measure::SECOND_POINT) {
		measure->setSecondPoint(pos);
		endMeasure();
	}
	
}
void MainWindow::pointPicked(QPoint p) {
	//works only on images with correct resolution and lens.
	Image image = project.images1[currentImage];
	if(!image.valid)
		return;

	QPointF pos = ui->graphicsView->mapToScene(p);
	
	QBrush blueBrush(Qt::blue);

	QPen outlinePen(Qt::white);
	outlinePen.setCosmetic(true);
	outlinePen.setWidth(5);
	auto borderPoint = new BorderPoint(-3, -3, 6, 6);
	borderPoint->setPos(pos.x(), pos.y());
	borderPoint->setPen(outlinePen);
	borderPoint->setBrush(blueBrush);
	borderPoint->setFlag(QGraphicsItem::ItemIsMovable);
	borderPoint->setFlag(QGraphicsItem::ItemIsSelectable);
	borderPoint->setFlag(QGraphicsItem::ItemSendsScenePositionChanges);
	borderPoint->setCursor(Qt::CrossCursor);
	scene->addItem(borderPoint);

	if(!project.balls.size())
		addSphere();

	auto item = ui->sphereList->selectedItems()[0];
	int id = item->data(Qt::UserRole).toInt();
	assert(project.balls.count(id));
	Ball *ball = project.balls[id];
	ball->border.push_back(borderPoint);

	updateBorderPoints();
}

void MainWindow::updateBorderPoints() {

	for(auto &it: project.balls) {
		//TODO make a ball function
		Ball *ball = it.second;

		ball->circle->setVisible(false);
		ball->smallcircle->setVisible(false);

		if(ball->border.size() >= 3) {
			bool fitted = ball->fit(project.imgsize);
			if(fitted) {
				QPointF c = ball->center;
				double R = double(ball->radius);
				double r = double(ball->smallradius);
				ball->circle->setRect(c.x()-R, c.y()-R, 2*R, 2*R);
				ball->circle->setVisible(true);
				ball->smallcircle->setRect(c.x()-r, c.y()-r, 2*r, 2*r);
				ball->smallcircle->setVisible(true);

			}
		}
	}
}

void MainWindow::updateHighlight() {
	if(ignore_scene_changes)
		return;

	size_t n = size_t(currentImage);
	for(auto &it: project.balls) {
		Ball *ball = it.second;
		if(!ball->highlight) continue;

		ball->highlight->setBrush(Qt::green);
		ball->lights[n] = ball->highlight->pos();
	}
}

void MainWindow::deleteSelected() {
	for(auto &it: project.balls) {
		Ball *ball = it.second;
		auto border = ball->border;
		ball->border.clear();
		std::copy_if (border.begin(), border.end(), std::back_inserter(ball->border), [border](QGraphicsEllipseItem *e) {
			bool remove = e->isSelected();
			if(remove) delete e;
			return !remove;
		});
		if(ball->highlight && ball->highlight->isSelected()) {
			ball->resetHighlight(currentImage);
			showHighlights(currentImage);
		}
	}
	updateBorderPoints();
}

void MainWindow::changeSphere(QListWidgetItem *current, QListWidgetItem */*previous*/) {

	for(auto ball: project.balls)
		ball.second->setActive(false);

	if(!current)
		return;

	int current_id = current->data(Qt::UserRole).toInt();
	if(!project.balls.count(current_id))
		throw QString("A sphere was not properly deleted!");
	project.balls[current_id]->setActive(true);
}

int MainWindow::addSphere() {
	for(auto &ball: project.balls)
		ball.second->setActive(false);

	ignore_scene_changes = true;
	std::set<int> used;

	for(int i = 0; i < ui->sphereList->count(); ++i)
		used.insert(ui->sphereList->item(i)->data(Qt::UserRole).toInt());

	int id = 0;;
	while(used.count(id))
		id++;
	Ball *ball = new Ball(project.size());
	project.balls[id] = ball;
	setupSphere(id, ball);
	ignore_scene_changes = false;
	return id;
}

void MainWindow::setupMeasures() {
	for(auto m: project.measures) {
		m->setScene(scene);
		m->setVisible(true);
	}
}

void MainWindow::setupSpheres() {
	for(auto b: project.balls) {
		int id = b.first;
		Ball *ball = b.second;
		setupSphere(id, ball);
	}
}

void MainWindow::setupSphere(int id, Ball *ball) {
	auto *item = new QListWidgetItem(QString("Shere %1").arg(id+1), ui->sphereList);
	item->setSelected(true);
	item ->setData(Qt::UserRole, id);

	QPen outlinePen(Qt::yellow);
	outlinePen.setCosmetic(true);
	ball->circle = scene->addEllipse(0, 0, 1, 1, outlinePen);
	ball->smallcircle = scene->addEllipse(0, 0, 1, 1, outlinePen);
	if(ball->center.isNull()) {
		ball->circle->setVisible(false);
		ball->smallcircle->setVisible(false);
	} else {
		QPointF c = ball->center;
		double R = double(ball->radius);
		double r = double(ball->smallradius);
		ball->circle->setRect(c.x()-R, c.y()-R, 2*R, 2*R);
		ball->circle->setVisible(true);
		ball->smallcircle->setRect(c.x()-r, c.y()-r, 2*r, 2*r);
		ball->smallcircle->setVisible(true);
	}


	auto high = new HighlightPoint(-2, -2, 2, 2);
	high->setVisible(false);
	QPen pen;
	pen.setColor(Qt::transparent);
	pen.setWidth(0);
	high->setPen(pen);
	high->setBrush(Qt::green);
	high->setFlag(QGraphicsItem::ItemIsMovable);
	high->setFlag(QGraphicsItem::ItemIsSelectable);
	high->setFlag(QGraphicsItem::ItemSendsScenePositionChanges);
	ball->highlight = high;
	scene->addItem(high);

	for(auto b: ball->border)
		scene->addItem(b);
}

void MainWindow::removeSphere() {
	for(auto a: ui->sphereList->selectedItems()) {
		int id = a->data(Qt::UserRole).toInt();
		assert(project.balls.count(id));
		Ball *ball = project.balls[id];
		delete ball;
		project.balls.erase(id);
	}
	qDeleteAll(ui->sphereList->selectedItems());
}

void MainWindow::imagesContextMenu(QPoint) {

}
void MainWindow::spheresContextMenu(QPoint pos) {
	auto *item = ui->sphereList->itemAt(pos);
	if(!item) return;
	ui->sphereList->setCurrentItem(item);

	QMenu myMenu;
	myMenu.addAction("Find highlights",  this, SLOT(processCurrentSphere()));
	myMenu.addAction("Insert", this, SLOT(addSphere()));
	myMenu.addSeparator();
	myMenu.addAction("RemoveThisSphere",  this, SLOT(removeSphere()));


	myMenu.exec(mapToGlobal(pos));
}


void MainWindow::processCurrentSphere() {
	for(auto a: ui->sphereList->selectedItems()) {
		int id = a->data(Qt::UserRole).toInt();
		assert(project.balls.count(id));
		Ball *ball = project.balls[id];
		if(!ball->fitted) {
			QMessageBox::critical(this, "Sorry can't do.", "This sphere has no center or radius!");
			return;
		}
		sphere_to_process = id;
	}
	process();
}

void MainWindow::process() {
	progress = new QProgressDialog("Looking for highlights...", "Cancel", 0, project.size(), this);

	QThreadPool::globalInstance()->setMaxThreadCount(1);
	progress_jobs.clear();
	for(size_t i = 0; i < project.size(); i++)
		progress_jobs.push_back(i);
	//0 -> ok, 1 -> could not open 2 -> flipped, 3-> wrong resolution
	QFuture<void> future = QtConcurrent::map(progress_jobs, [&](int i) -> int { return processImage(i); });
	watcher.setFuture(future);
	connect(&watcher, SIGNAL(finished()), this, SLOT(finishedProcess()));
	connect(&watcher, SIGNAL(progressValueChanged(int)), progress, SLOT(setValue(int)));
	connect(progress, SIGNAL(canceled()), this, SLOT(cancelProcess()));
	progress->setWindowModality(Qt::WindowModal);
}

void MainWindow::cancelProcess() {
	watcher.cancel();
}

void MainWindow::finishedProcess() {
	if(notloaded.size() || flipped.size() || resolution.size()) {
		if(notloaded.size())
			QMessageBox::critical(this, "Houston we have a problem!", "Could not load images: " + notloaded.join(", "));
		if(resolution.size())
			QMessageBox::critical(this, "Houston we have a problem!", "These images have different sizes: "+ resolution.join(", "));
		if(flipped.size())
			QMessageBox::critical(this, "Houston we have a problem!", "These images are probably just rotated: " + flipped.join(", "));
		return;
	}

#ifdef NDEBUG
	//histogram for highlight threshold
	for(auto it: project.balls) {
		Ball *ball = it.second;
		cout << "Ball " << it.first << "\n";
		for(size_t i = 0; i < ball->histogram.size(); i++) {
			cout << "Light: " << i << " ";
			for(int n: ball->histogram[i])
				cout << n << " ";
			cout << "\n";
		}
		cout << endl;
	}
#endif
	project.computeDirections();
	auto selected = ui->imageList->selectedItems();
	if(selected.size() == 0)
		return;
	openImage(selected[0]);
}

int MainWindow::processImage(int n) {
	if(project.images1[size_t(n)].skip) return 0;

	QString filename = project.images1[n].filename;
	QImage img(project.dir.filePath(filename));
	if(img.isNull()) {
		notloaded.push_back(project.images1[n].filename);
		return 0;
	}
	if(img.size() != project.imgsize) {
		if(img.size() == project.imgsize.transposed())
			flipped.push_back(project.images1[n].filename);
		else
			resolution.push_back(project.images1[n].filename);
		return 0;
	}


	for(auto &it: project.balls) {
		if(sphere_to_process != -1 && sphere_to_process != it.first)
			continue;
		if(it.second->fitted) {
			it.second->findHighlight(img, n);
		}
	}
	return 1;
}

void MainWindow::quit() {
	int res = QMessageBox::question(this, "Closing relight.", "Sure?");
	if(res == QMessageBox::Yes)
		exit(0);
}

/*load LP:
   if no images:
		   look for images in the directory of the LP,
				 if no images found ask for directory with images.
					 if wrong number: complain and ask for again/cancel.
					 if image names not matching jwarn and -> use order or cancel.
					 load images
	if images:
			wrong number: complain
			wrong names ->use order or cancel
*/


void MainWindow::loadLP() {
	QString lp = QFileDialog::getOpenFileName(this, "Select light direction file", QString(), "Light directions (*.lp)");
	if(lp.isNull())
		return;
	loadLP(lp);
}

void MainWindow::loadLP(QString lp) {
	vector<QString> filenames;
	std::vector<Vector3f> directions;

	try {
		ImageSet::parseLP(lp, directions, filenames);

	} catch(QString error) {
		QMessageBox::critical(this, "LP file invalid: ", error);
		return;
	}

	if(project.size() != filenames.size()) {
		auto response = QMessageBox::question(this, "Light directions and images",
			QString("The folder contains %1 images, the .lp file specify %1 images.\n")
					.arg(project.size()).arg(filenames.size()));
		if(response == QMessageBox::Cancel || response == QMessageBox::No)
			return;
	}

	vector<Vector3f> ordered_dir(directions.size());
	bool success = true;
	for(size_t i = 0; i < filenames.size(); i++) {
		QString &s = filenames[i];
		int pos = project.indexOf(s);
		if(pos == -1) {
			success = false;
			break;
		}
		ordered_dir[pos] = directions[i];
	}

	if(success) {
		for(size_t i = 0; i < project.size(); i++)
			project.images1[i].direction = ordered_dir[i];
	} else {
		auto response = QMessageBox::question(this, "Light directions and images",
			"Filenames in .lp do not match with images in the .lp directory. Do you want to just use the filename order?");
		if(response == QMessageBox::Cancel || response == QMessageBox::No)
			return;
		for(size_t i = 0; i < project.size(); i++)
			project.images1[i].direction = directions[i];
	}
}
void MainWindow::saveLPs() {
	int count = 1;
	QString basename = "sphere";
	for(auto it: project.balls) {
		QString filename = basename;
		filename += QString::number(count);
		filename += ".lp";
		count++;

		Ball *ball = it.second;
		filename = project.dir.filePath(filename);
		ball->computeDirections();
		project.saveLP(filename, ball->directions);
	}
	QMessageBox::information(this, "Saved LPs", QString("Saved %1 .lp's in images folder.").arg(project.balls.size()));
	project.computeDirections();

	vector<Vector3f> directions;
	for(auto img: project.images1)
		directions.push_back(img.direction);

	project.saveLP(basename + ".lp", directions);
}

void MainWindow::exportRTI() {
	if(project.balls.size())
		project.computeDirections();

	QStringList nodir;
	for(size_t i = 0; i < project.size(); i++) {
		if(project.images1[i].skip)
			continue;

		if(project.images1[i].direction.isZero())
			nodir.push_back(project.images1[i].filename);
	}
	if(nodir.size()) {
		QMessageBox::critical(this, "Could not export RTI.", "Some images lack a light direction: " + nodir.join(", "));
		return;
	}


	//should init with saved preferences.
	rtiexport->setImages(project.images());

	rtiexport->showImage(imagePixmap->pixmap());
	rtiexport->lights = project.directions();
	rtiexport->path = project.dir.path();
	rtiexport->setModal(true);
	rtiexport->show();
	//this needs to be called AFTER show, to ensure proportions are computed properly
	rtiexport->setCrop(project.crop);
	rtiexport->exec();
	project.crop = rtiexport->crop;
}

void MainWindow::viewRTI() {
	QString output = QFileDialog::getExistingDirectory(this, "Select an output directory");
	if(output.isNull()) return;

	HttpServer &server = HttpServer::instance();
	server.stop();
	server.port = QSettings().value("port", 8080).toInt();
	server.start(output);
	server.show();
}

void MainWindow::showQueue() {
	if(!queue)
		queue = new QueueWindow(this);

	queue->show();
	queue->raise();
}

void MainWindow::editLensParameters() {
	FocalDialog *focal = new FocalDialog(&project, this);
	focal->setWindowModality(Qt::WindowModal);
	bool result = focal->exec();
	delete focal;
}


void MainWindow::showHelp() {
	help->show();
//	help->setUrl(":/docs/help.html");
}

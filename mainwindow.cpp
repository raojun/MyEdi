#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "mdichild.h"
#include <QMdiSubWindow>
#include <QFileDialog>
#include <QSignalMapper>
#include <QSettings>
#include <QCloseEvent>

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    //创建间隔器动作并在其中设置间隔器
    actionSeparator =new QAction(this);
    actionSeparator->setSeparator(true);

    updateMenus();//更新菜单

    //当有活动窗口时更新菜单
    connect(ui->mdiArea,SIGNAL(subWindowActivated(QMdiSubWindow*)),this,SLOT(updatMenus()));

    readSettings();//初始窗口时读取窗口设置信息

}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_actionNew_triggered()
{
    //创建MdiClild
    MdiChild *child=createMdiChild();
    child->newFile();//新建文件
    child->show();//显示子窗口

    //创建信号映射器
    windowMapper =new QSignalMapper(this);
    //映射器重新发射信号，根据信号设置活动窗口
    connect(windowMapper,SIGNAL(mapped(QWidget*)),this,SLOT(setActiveSubWindow(QWidget*)));
    //更新窗口菜单，并且设置当窗口菜单将要现实的时候更新窗口菜单
    updateWindowMenu();
    connect(ui->menuW,SIGNAL(aboutToShow()),this,SLOT(updateWindowMenu()));
}

void MainWindow::updateMenus()
{
    //根据是否有活动窗口来设置各个动作是否可用
    bool hasMdiChild=(activeMdiChild()!=0);
    ui->actionSave->setEnabled(hasMdiChild);
    ui->actionSaveAs->setEnabled(hasMdiChild);
    ui->actionPaste->setEnabled(hasMdiChild);
    ui->actionClose->setEnabled(hasMdiChild);
    ui->actionCloseAll->setEnabled(hasMdiChild);
    ui->actionTile->setEnabled(hasMdiChild);
    ui->actionCascade->setEnabled(hasMdiChild);
    ui->actionNext->setEnabled(hasMdiChild);
    ui->actionPrevious->setEnabled(hasMdiChild);

    //设置间隔器是否显示
    actionSeparator->setVisible(hasMdiChild);

    //有活动窗口且有被选择的文本，剪切复制才可用
    bool hasSelection=(activeMdiChild()&&activeMdiChild()->textCursor().hasSelection());
    ui->actionCut->setEnabled(hasSelection);
    ui->actionCopy->setEnabled(hasSelection);

    //有活动窗口且文档有撤消操作时撤消动作可用
    ui->actionUndo->setEnabled(activeMdiChild()&&activeMdiChild()->document()->isUndoAvailable());
    ui->actionRedo->setEnabled(activeMdiChild()&&activeMdiChild()->document()->isRedoAvailable());
}

void MainWindow::updateWindowMenu()
{
    //先清空菜单，然后再添加各个菜单动作
    ui->menuW->clear();
    ui->menuW->addAction(ui->actionClose);
    ui->menuW->addAction(ui->actionCloseAll);
    ui->menuW->addSeparator();
    ui->menuW->addAction(ui->actionTile);
    ui->menuW->addAction(ui->actionCascade);
    ui->menuW->addSeparator();
    ui->menuW->addAction(ui->actionNext);
    ui->menuW->addAction(ui->actionPrevious);
    ui->menuW->addAction(actionSeparator);

    //如果有活动窗口，则显示间隔器
    QList<QMdiSubWindow *>windows=ui->mdiArea->subWindowList();
    actionSeparator->setVisible(!windows.isEmpty());

    //遍历各个子窗口
    for(int i=0;i<windows.size();++i)
    {
        MdiChild *child=qobject_cast<MdiChild *>(windows.at(i)->widget());
        QString text;
        //如果窗口数小于9，则设置编号为快捷键
        if(i<9)
        {
            text=tr("&%1 %2").arg(i+1).arg(child->userFriendlyCurrentFile());
        }
        else
        {
            text=tr("%1 %2").arg(i+1).arg(child->userFriendlyCurrentFile());
        }
        //添加动作到菜单，设置动作可以选择
        QAction *action=ui->menuW->addAction(text);
        action->setCheckable(true);
        //设置当前活动窗口为选中状态
        action->setChecked(child==activeMdiChild());
        //关联动作的触发信号到信号映射器的map()槽，这个槽会发射mapped()信号
        connect(action,SIGNAL(triggered()),windowMapper,SLOT(map()));
        //将动作与响应的窗口部件进行映射
        //在发射mapped()信号时就会以这个窗口部件为参数
        windowMapper->setMapping(action,windows.at(i));
    }
}

MdiChild *MainWindow::activeMdiChild()
{
    //如果有活动窗口，则将其内的中心部件转换为MdiChild类型，没有则直接返回0
    if(QMdiSubWindow *activeSubWindow=ui->mdiArea->activeSubWindow())
        return qobject_cast<MdiChild *>(activeSubWindow->widget());
    return 0;
}

MdiChild *MainWindow::createMdiChild()
{
    //创建MdiChild部件
    MdiChild *child=new MdiChild;
    //想多文档区域添加子窗口，child为中心部件
    ui->mdiArea->addSubWindow(child);

    //根据QTextEdit类的是否可以复制信号设置剪切复制是否可用
    connect(child,SIGNAL(copyAvailable(bool)),ui->actionCut,SLOT(setEnabled(bool)));
    connect(child,SIGNAL(copyAvailable(bool)),ui->actionCopy,SLOT(setEnabled(bool)));
    //根据QTextDocument类的是否可以撤销恢复信号设置撤销恢复动作是否可用
    connect(child->document(),SIGNAL(undoAvailable(bool)),ui->actionUndo,SLOT(setEnabled(bool)));
    connect(child->document(),SIGNAL(redoAvailable(bool)),ui->actionRedo,SLOT(setEnabled(bool)));
    return child;
}

void MainWindow::on_actionOpen_triggered()
{
    //获取文件路径
    QString fileName=QFileDialog::getOpenFileName(this);

    //如果路径不为空，则查看该文件是否已经打开
    if(!fileName.isEmpty())
    {
        QMdiSubWindow *existing=findMdiChild(fileName);
        //如果已经存在，则将对应的子窗口设置为活动窗口
        if(existing)
        {
            ui->mdiArea->setActiveSubWindow(existing);
            return ;
        }
        //如果没有打开，则新建子窗口
        MdiChild *child=createMdiChild();
        if(child->loadFile(fileName))
        {
            ui->statusBar->showMessage("打开文件成功",2000);
            child->show();
        }
        else
        {
            child->close();
        }
    }
}

//查找子窗口实现函数
QMdiSubWindow * MainWindow::findMdiChild(const QString &fileName)
{
    QString canonicalFilePath=QFileInfo(fileName).canonicalFilePath();
    //利用foreach语句遍历子窗口列表，如果其文件路径和要查找的路径相同，则返回该窗口
    foreach (QMdiSubWindow *window,ui->mdiArea->subWindowList()) {
        MdiChild *mdiChild=qobject_cast<MdiChild *>(window->widget());
        if(mdiChild->currentFile()==canonicalFilePath)
            return window;
    }
    return 0;
}

//将传递过来的窗口部件设置为活动窗口
void MainWindow::setActiveSubWindow(QWidget *window)
{
    //如果传递了窗口部件，则将其设置为活动窗口
    if(!window)
    {
        return ;
    }
    ui->mdiArea->setActiveSubWindow(qobject_cast<QMdiSubWindow *>(window));

}

//保存信号槽
void MainWindow::on_actionSave_triggered()
{
    if(activeMdiChild()&&activeMdiChild()->save())
        ui->statusBar->showMessage("文件保存成功",2000);
}

//另存为信号槽
void MainWindow::on_actionSaveAs_triggered()
{
    if(activeMdiChild()&&activeMdiChild()->saveAs())
        ui->statusBar->showMessage("文件保存成功",2000);
}
//撤销信号槽
void MainWindow::on_actionUndo_triggered()
{
    if(activeMdiChild())
        activeMdiChild()->undo();
}
//恢复信号槽
void MainWindow::on_actionRedo_triggered()
{
    if(activeMdiChild())
        activeMdiChild()->redo();
}
//剪切信号槽
void MainWindow::on_actionCut_triggered()
{
    if(activeMdiChild())
        activeMdiChild()->cut();
}
//复制信号槽
void MainWindow::on_actionCopy_triggered()
{
    if(activeMdiChild())
        activeMdiChild()->copy();
}
//粘贴信号槽
void MainWindow::on_actionPaste_triggered()
{
    if(activeMdiChild())
        activeMdiChild()->paste();
}
//关闭信号槽
void MainWindow::on_actionClose_triggered()
{
    ui->mdiArea->closeActiveSubWindow();
}

//写入窗口设置
void MainWindow::writeSettings()
{
    QSettings settings("yafeiliunx","myMdi");
    //写入位置信息和大小信息
    settings.setValue("pos",pos());
    settings.setValue("size",size());
}

//读取窗口设置
void MainWindow::readSettings()
{
    QSettings settings("yafeilinux","myMdi");
    QPoint pos=settings.value("pos",QPoint(200,200)).toPoint();
    QSize size=settings.value("size",QSize(400,400)).toSize();
    move(pos);
    resize(size);
}

//关闭事件处理函数的定义
void MainWindow::closeEvent(QCloseEvent *event)
{
    //先执行多文档区域的关闭操作
    ui->mdiArea->closeAllSubWindows();
    //如果该有窗口没有关闭，则忽略该事件
    if(ui->mdiArea->currentSubWindow())
    {
        event->ignore();
    }
    else
    {
        //在关闭前写入窗口设置
        writeSettings();
        event->accept();
    }
}

//退出信号槽
void MainWindow::on_actionExit_triggered()
{
    //等价于QApplication::closeAllWindows();
    qApp->closeAllWindows();
}


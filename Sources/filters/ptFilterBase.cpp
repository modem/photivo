/*******************************************************************************
**
** Photivo
**
** Copyright (C) 2012 Bernd Schoeler <brjohn@brother-john.net>
** Copyright (C) 2012 Michael Munzert <mail@mm-log.com>
**
** This file is part of Photivo.
**
** Photivo is free software: you can redistribute it and/or modify
** it under the terms of the GNU General Public License version 3
** as published by the Free Software Foundation.
**
** Photivo is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with Photivo.  If not, see <http://www.gnu.org/licenses/>.
**
*******************************************************************************/

#include <QWidget>
#include <QVBoxLayout>
#include <QMap>

#include "ptInfo.h"

#include "ptCfgItem.h"
#include "ptFilterBase.h"
#include "ptFilterFactory.h"
#include "ptFilterDM.h"
#include <ptCheck.h>
#include <ptChoice.h>
#include <ptWidget.h>
#include <ptInput.h>
#include <ptToolBox.h>
#include <ptCurveWindow.h>

#include <ptSettings.h>

//==============================================================================

const QString CCustomStores = "CustomStores";
const QString CIsBlocked    = "isBlocked";

//==============================================================================

ptFilterBase::~ptFilterBase() {
  if(FGuiContainer) {
    /* NOTE: Unfortunately Qt combines GUI parent and resource ownership. Might cause problems
    with destruction order when Photivo closes down. We don’t want the main UI trying to access
    a filter GUI widget that doesn’t exist anymore – and vice versa. Definitely needs thorough
    testing.*/
    FGuiContainer->setParent(nullptr);
    DelAndNull(FGuiContainer);
  }
}

//==============================================================================

void ptFilterBase::exportPreset(QSettings *APreset, const bool AIncludeFlags /*= true*/) const {
  APreset->beginGroup(this->FFilterName + "/" + this->uniqueName());
  APreset->remove("");   // remove potentially existing settings in AIni

  // store default TFilterConfig from FCfgItems list
  for (ptCfgItem hSetting: FCfgItems) {
    if (hSetting.Storeable) {
      auto hVariant = FConfig->getValue(hSetting.Id);

      // Convert bool to int because that is more robust in a preset file
      if (hVariant.type() == QVariant::Bool)
        hVariant.convert(QVariant::Int);

      APreset->setValue(hSetting.Id, hVariant);
    }
  }

  // store additional custom config lists (e.g. used for curve anchors)
  QStringList hStoreIds = FConfig->storeIds();
  if (!hStoreIds.isEmpty()) {
    APreset->setValue(CCustomStores, FConfig->storeIds());
    for (QString hId: FConfig->storeIds()) {
      TConfigStore *hList = FConfig->getStore(hId);
      APreset->beginGroup(hId);
      for (auto hItem = hList->constBegin(); hItem != hList->constEnd(); ++hItem) {
        APreset->setValue(hItem.key(), hItem.value());
      }
      APreset->endGroup();
    }
  }

  if (AIncludeFlags) {
    APreset->setValue(CIsBlocked, (int)FIsBlocked);
  }

  doExportCustomConfig(APreset, AIncludeFlags);

  APreset->endGroup();
}

//==============================================================================

void ptFilterBase::importPreset(QSettings *APreset, const bool ARequestPipeRun /*=false*/) {
  APreset->beginGroup(this->FFilterName + "/" + this->uniqueName());

  // *** default config store *** //
  TConfigStore hSettings;
  for (ptCfgItem hCfgItem: FCfgItems) {
    auto hValue = APreset->value(hCfgItem.Id);
    if (hValue.isValid()) {
      // valid hValue means the settings entry was present in AIni
      ensureVariantType(hValue, variantType(hCfgItem));
      hSettings.insert(hCfgItem.Id, hValue);
    }
  }

  // *** custom stores *** //
  // Read list of store names, create and fill each one
  for (QString hId: APreset->value(CCustomStores).toStringList()) {
    TConfigStore *hList = FConfig->getStore(hId);
    if (!hList)
      hList = FConfig->newStore(hId);

    APreset->beginGroup(hId);
    for (QString hKey: APreset->allKeys()) {
      hList->insert(hKey, APreset->value(hKey));  // read one store item from preset
    }
    APreset->endGroup();
  }

  // flags and derived’s custom stuff
  if (flags() & FilterIsBlockable) {
    FIsBlocked = APreset->value(CIsBlocked, FIsBlocked).toBool() ||
                 Settings->GetStringList("HiddenTools").contains(FUniqueName);
  }
  doImportCustomConfig(APreset);

  APreset->endGroup();

  FConfig->update(hSettings);
  updateGui(ARequestPipeRun);
}

//==============================================================================

void ptFilterBase::reset(const bool ARequestPipeRun /*=false*/) {
  // Call the children reset method
  doReset();

  FConfig->clearCustomStores();
  createConfig();
  updateGui(ARequestPipeRun);
}

//==============================================================================

void ptFilterBase::runFilter(ptImage *AImage) const {
  doRunFilter(AImage);
}

//==============================================================================

bool ptFilterBase::setBlocked(const bool AIsBlocked) {
  if (!(flags() & FilterIsBlockable))
    return false;

  if (AIsBlocked != FIsBlocked) {
    FIsBlocked          = AIsBlocked;
    bool hStatusChanged = this->checkActiveChanged(true);

    GFilterDM->UpdateActivesList(this);

    // GUI update (if we have a GUI) must happen before the possible pipe run.
    if (FGuiContainer)
      FGuiContainer->updateGui();

    if (hStatusChanged || FIsActive)
      requestPipeRun(true);
  }

  return true;
}

//==============================================================================

bool ptFilterBase::isHidden() const {
  return Settings->GetStringList("HiddenTools").contains(FUniqueName);
}

//==============================================================================

bool ptFilterBase::setHidden(const bool AIsHidden) {
  if (!(flags() & FilterIsHideable))
    return false;

  // update global list of hidden tools
  auto hHiddens  = Settings->GetStringList("HiddenTools");
  if (hHiddens.contains(FUniqueName) != AIsHidden) {
    if (AIsHidden) {
      if (!hHiddens.contains(FUniqueName))
        hHiddens.append(FUniqueName);
    } else {
      hHiddens.removeAll(FUniqueName);
    }    
    Settings->SetValue("HiddenTools", hHiddens);

    bool hStatusChanged = this->checkActiveChanged(true);
    GFilterDM->UpdateActivesList(this);

    if (hStatusChanged || FIsActive)
      requestPipeRun(true);
  }

  return true;
}

//==============================================================================

bool ptFilterBase::isFavourite() const {
  return Settings->GetStringList("FavouriteTools").contains(FUniqueName);
}

//==============================================================================

bool ptFilterBase::setFavourite(const bool AIsFavourite) {
  if (!(flags() & FilterIsFavouriteable))
    return false;

  auto hFavs = Settings->GetStringList("FavouriteTools");
  if (!AIsFavourite) {
    hFavs.removeAll(FUniqueName);
  } else {
    if (!hFavs.contains(FUniqueName))
      hFavs.append(FUniqueName);
  }

  Settings->SetValue("FavouriteTools", hFavs);
  return true;
}

//==============================================================================

QString ptFilterBase::uniqueName() const {
  GInfo->Assert(!FUniqueName.isEmpty(),
                QString("No unique name set for filter \"%1\".") .arg(FFilterName), AT);

  return FUniqueName;
}

//==============================================================================

ptToolBox *ptFilterBase::gui() {
  if (!FGuiContainer)
    createGui();

  return FGuiContainer;
}

//==============================================================================

// Read .h regarding this function. ... I mean it!
QWidget *ptFilterBase::guiWidget() {
  return gui();
}

// Read .h regarding this function. ... I mean it!
bool ptFilterBase::canHide() const {
  return flags() & FilterIsHideable;
}

//==============================================================================

void ptFilterBase::setPos(const int ATab, const int AIdx) {
  FParentTabIdx   = ATab;
  FIdxInParentTab = AIdx;
  GFilterDM->UpdatePositions(this);
}

//==============================================================================

void ptFilterBase::init(const QString &AUniqueName, const QString &AGuiNamePostfix) {
  GInfo->Assert(!AUniqueName.isEmpty(),
                QString("Unique name for filter \"%1\" cannot be empty.").arg(FFilterName), AT);

  if (FGuiContainer)
    FGuiContainer->setObjectName(AUniqueName);

  this->setObjectName(AUniqueName);
  FUniqueName     = AUniqueName;
  FCaptionPostfix = AGuiNamePostfix;
}

//==============================================================================

ptFilterBase::TFilterFlags ptFilterBase::flags() const {
  return FilterIsBlockable | FilterHasDefault | FilterIsSaveable | FilterIsFavouriteable |
         FilterIsHideable;
}

//==============================================================================

ptFilterBase::ptFilterBase()
: /*QObject(),*/ptTempFilterBase(),
  FHasActiveCfg(false),
  FIsSlow(false),
  FConfig(make_unique<ptFilterConfig>()),
  FGuiContainer(nullptr),
  FIsActive(false),
  FParentTabIdx(-1),
  FIdxInParentTab(-1),
  FIsBlocked(false)
{}

//==============================================================================

void ptFilterBase::internalInit() {
  doDefineControls();

  createConfig();
  checkActiveChanged();
}

//==============================================================================

void ptFilterBase::connectCommonDispatch() {
  GInfo->Assert(!FGuiContainer, "The filter's ("+FFilterName+") GUI must be created first.", AT);

  for (auto hCfgItem: FCfgItems)
    performCommonConnect(hCfgItem, FGuiContainer->findChild<QWidget*>(hCfgItem.Id));
}

//==============================================================================

int ptFilterBase::cfgIdx(const QString &AId) const {
  for (int i = 0; i < FCfgItems.size(); ++i) {
    if (FCfgItems.at(i).Id == AId) {
      return i;
    }
  }
  return -1;
}

//==============================================================================

void ptFilterBase::initDesignerGui(QWidget *AGuiBody) {
  for (ptCfgItem hCfgItem: FCfgItems) {
    // find the widget for each config item
    ptWidget *hWidget = AGuiBody->findChild<ptWidget*>(hCfgItem.Id);
    if (!hWidget)
      GInfo->Raise(QString("Widget \"%1\" not found.").arg(hCfgItem.Id),
                   QString(FFilterName+"/"+FUniqueName+": "+AT).toAscii().data());

    // init the widget with default values and connect signals/slots
    hWidget->init(hCfgItem);
    this->performCommonConnect(hCfgItem, hWidget);
  }
}

//==============================================================================

/*static*/
ptWidget* ptFilterBase::createWidgetByType(const ptCfgItem &ACfgItem, QWidget *AParent) {
  switch (ACfgItem.Type) {
  // NOTE: Buttons will probably be removed
//      case ptCfgItem::Button: {
//        hGuiWidget = new QToolButton(FGuiContainer);
//        break;
//      }
  case ptCfgItem::Check:      return new ptCheck(ACfgItem, AParent);
  case ptCfgItem::Combo:      return new ptChoice(ACfgItem, AParent);
  case ptCfgItem::SpinEdit:   // fall through
  case ptCfgItem::Slider:     // fall through
  case ptCfgItem::HueSlider:  return new ptInput(ACfgItem, AParent);
  case ptCfgItem::CurveWin:   return new ptCurveWindow(ACfgItem, AParent);

  default:
    GInfo->Raise(QString("Unhandled ptCfgItem::TType value: %1.").arg(ACfgItem.Type), AT);
    break;
  }

  return nullptr;
}

//==============================================================================

void ptFilterBase::commonDispatch(const QString AId, const QVariant ANewValue) {
  if (!ANewValue.isValid()) return;

  // find the right config item
  int hIdx = cfgIdx(AId);
  if (hIdx == -1) return;

  if ((FCfgItems.at(hIdx).Type < ptCfgItem::CFirstCustomType) &&
      (FConfig->getValue(AId) != ANewValue))
  { // item from default store
    FConfig->setValue(AId, ANewValue);
    requestPipeRun();

  } else if ((FCfgItems.at(hIdx).Type >= ptCfgItem::CFirstCustomType) &&
             (ANewValue.type() == QVariant::Map))
  { // item that has a custom store
    (*FConfig->getStore(FCfgItems.at(hIdx).Id)) = ANewValue.toMap();
    requestPipeRun();
  }
}

//==============================================================================

void ptFilterBase::createConfig() {
  TConfigStore hDefaultStore;

  // NOTE: Not sure yet if buttons really need to be stored in settings.
  // If not add exclusion code to for loop.

  for (ptCfgItem hCfgItem: FCfgItems) {
    if (hCfgItem.Type < ptCfgItem::CFirstCustomType)
      hDefaultStore.insert(hCfgItem.Id, hCfgItem.Default);
     else
      FConfig->newStore(hCfgItem.Id, hCfgItem.Default.toMap());
  }

  doAddCustomConfig(hDefaultStore);
  FConfig->init(hDefaultStore);
}

//==============================================================================

void ptFilterBase::performCommonConnect(const ptCfgItem &ACfgItem, QObject *AObject) {
  if (ACfgItem.UseCommonDispatch && AObject) {
    connect(AObject, SIGNAL(valueChanged(QString,QVariant)),
            this,    SLOT(commonDispatch(QString,QVariant)));
  }
}

//==============================================================================

bool ptFilterBase::checkActiveChanged(const bool ANoSignal /*= false*/) {
  bool hOldStatus     = FIsActive;
  FHasActiveCfg       = doCheckHasActiveCfg();
  FIsActive           = FHasActiveCfg && !FIsBlocked && !isHidden();

  if (FIsActive != hOldStatus) {
    GFilterDM->UpdateActivesList(this);
    if (!ANoSignal)
      emit activityChanged();
    return true;

  } else {
    return false;
  }
}

//==============================================================================

void Update(const QString GuiName);
void ptFilterBase::requestPipeRun(const bool AUnconditional) {
  if (AUnconditional || this->checkActiveChanged() || FIsActive)
    Update(FUniqueName);
}

//==============================================================================

void ptFilterBase::updateGui(const bool ARequestPipeRun /*= true*/) {
  if (FGuiContainer) {
    // call the children.
    doUpdateGui();

    for(ptCfgItem hCfgItem: FCfgItems) {
      ptWidget* hWidget = FGuiContainer->findChild<ptWidget*>(hCfgItem.Id);

      if (!hWidget) {
        GInfo->Warning(QString("%1: Widget \"%2\" not found in GUI.")
                          .arg(uniqueName(), hCfgItem.Id), AT);

      } else if (hCfgItem.Type >= ptCfgItem::CFirstCustomType) {
        hWidget->setValue(*FConfig->getStore(hCfgItem.Id));

      } else {
        hWidget->setValue(FConfig->getValue(hCfgItem.Id));
      }
    }
  }

  if (ARequestPipeRun) {
    requestPipeRun();
  } else {
    checkActiveChanged();
  }
}

//==============================================================================

void ptFilterBase::createGui() {
  GInfo->Assert(!FGuiContainer, "GUI already created.", AT);
  GInfo->Assert((bool)FConfig, "FSettings object must be instantiated first.", AT);

  auto hGuiBody = doCreateGui();

  // We create the gui, if the children did not do it.
  if (!hGuiBody) {
    hGuiBody     = new QWidget;
    auto hLayout = new QVBoxLayout(hGuiBody);

    for (ptCfgItem hCfgItem: FCfgItems) {
      ptWidget *hGuiWidget = createWidgetByType(hCfgItem, hGuiBody);
      if (hGuiWidget) {
        hLayout->addWidget(hGuiWidget);
        performCommonConnect(hCfgItem, hGuiWidget);
      }
    }
  }

  FGuiContainer = new ptToolBox(hGuiBody, this);
  if (Settings->GetStringList("HiddenTools").contains(FUniqueName)) {
    FGuiContainer->hide();
  }
}

//==============================================================================

void ptFilterBase::ensureVariantType(QVariant &AValue, const QVariant::Type &AIntendedType) {
  if (AValue.type() != AIntendedType) {
    if (!AValue.convert(AIntendedType)) {
      GInfo->Raise(QString("Could not cast QVariant with value \"%1\" from type \"%2\" to "
                           "type \"%3\" in filter \"%4\" (filter class: %5).")
                   .arg(AValue.toString()).arg((int)AValue.type()).arg((int)AIntendedType)
                   .arg(FFilterName, FUniqueName), AT);
    }
  }
}

//==============================================================================

QVariant::Type ptFilterBase::variantType(const ptCfgItem &ACfgItem) {
  if ((ACfgItem.Type == ptCfgItem::Check) ||
      (ACfgItem.Type == ptCfgItem::Combo) ||
      (ACfgItem.Decimals == 0))
  {
    return QVariant::Int;
  }

  if (ACfgItem.Decimals > 0)
    return QVariant::Double;

  GInfo->Raise(QString("Could not determine data type of \"%1\" in filter \"%2\" (filter class: %3).")
               .arg(ACfgItem.Id, FFilterName, FUniqueName));
  return QVariant::Invalid;  // never reached but needed to shut up compiler warning
}



//==============================================================================



RegisterHelper::RegisterHelper(const ptFilterFactoryMethod AMethod, const QString AName) {
  ptFilterFactory::GetInstance()->RegisterFilter(AMethod, AName);
}

//==============================================================================

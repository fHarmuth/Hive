/*
* Copyright 2017-2018, Emilien Vallot, Christophe Calmejane and other contributors

* This file is part of Hive.

* Hive is free software: you can redistribute it and/or modify
* it under the terms of the GNU Lesser General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.

* Hive is distributed in the hope that it will be usefu_state,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU Lesser General Public License for more details.

* You should have received a copy of the GNU Lesser General Public License
* along with Hive.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "nodeTreeWidget.hpp"

#include <la/avdecc/controller/internals/avdeccControlledEntity.hpp>
#include <la/avdecc/logger.hpp>
#include "avdecc/controllerManager.hpp"
#include "avdecc/helper.hpp"
#include "toolkit/textEntry.hpp"
#include "toolkit/comboBox.hpp"
#include "nodeTreeDynamicWidgets/audioUnitDynamicTreeWidgetItem.hpp"
#include "nodeTreeDynamicWidgets/avbInterfaceDynamicTreeWidgetItem.hpp"
#include "nodeTreeDynamicWidgets/streamDynamicTreeWidgetItem.hpp"
#include "nodeTreeDynamicWidgets/streamPortDynamicTreeWidgetItem.hpp"
#include "nodeTreeDynamicWidgets/memoryObjectDynamicTreeWidgetItem.hpp"
#include "entityLogoCache.hpp"

#include <vector>
#include <utility>
#include <algorithm>

#include <QListWidget>
#include <QHeaderView>

#include "painterHelper.hpp"

class Label : public QWidget
{
	Q_OBJECT
	static constexpr int size{16};
	static constexpr int halfSize{size/2};
	
public:
	Label(QWidget* parent = nullptr)
		: QWidget{parent}
		, _backgroundPixmap{size, size}
	{
		QColor evenColor{0x5E5E5E};
		QColor oddColor{0xE5E5E5};

		evenColor.setAlpha(96);
		oddColor.setAlpha(96);
	
		QPainter painter{&_backgroundPixmap};
		painter.fillRect(_backgroundPixmap.rect(), evenColor);
		painter.fillRect(QRect{0, 0, halfSize, halfSize}, oddColor);
		painter.fillRect(QRect{halfSize, halfSize, halfSize, halfSize}, oddColor);

		connect(&_downloadButton, &QPushButton::clicked, this, &Label::clicked);

		_layout.addWidget(&_downloadButton);
	}
	
	Q_SIGNAL void clicked();
	
	void setImage(QImage const& image)
	{
		_image = image;
		_downloadButton.setVisible(image.isNull());
		repaint();
	}
	
protected:	
	virtual void paintEvent(QPaintEvent* event) override
	{
		if (_image.isNull())
		{
			QWidget::paintEvent(event);
		}
		else
		{
			QPainter painter{this};
			painter.fillRect(rect(), QBrush{_backgroundPixmap});
			painterHelper::drawCentered(&painter, rect(), _image);
		}
	}
	
private:
	QHBoxLayout _layout{this};
	QPushButton _downloadButton{"Click to Download", this};
	QPixmap _backgroundPixmap;
	QImage _image;
};

class NodeTreeWidgetPrivate : public QObject, public NodeVisitor
{
	Q_OBJECT
public:
	NodeTreeWidgetPrivate(NodeTreeWidget* q)
		: q_ptr(q)
	{
		auto& controllerManager = avdecc::ControllerManager::getInstance();
		connect(&controllerManager, &avdecc::ControllerManager::controllerOffline, this, &NodeTreeWidgetPrivate::controllerOffline);
		connect(&controllerManager, &avdecc::ControllerManager::entityOnline, this, &NodeTreeWidgetPrivate::entityOnline);
		connect(&controllerManager, &avdecc::ControllerManager::entityOffline, this, &NodeTreeWidgetPrivate::entityOffline);
	}

	Q_SLOT void controllerOffline()
	{
		Q_Q(NodeTreeWidget);

		q->setNode(la::avdecc::UniqueIdentifier{}, {});
		q->clearSelection();
	}

	Q_SLOT void entityOnline(la::avdecc::UniqueIdentifier const entityID)
	{
	}

	Q_SLOT void entityOffline(la::avdecc::UniqueIdentifier const entityID)
	{
		if (_controlledEntityID == entityID)
		{
			Q_Q(NodeTreeWidget);
			q->setNode(la::avdecc::UniqueIdentifier{}, {});
			q->clearSelection();
		}
	}

	void setNode(la::avdecc::UniqueIdentifier const entityID, AnyNode const& node)
	{
		Q_Q(NodeTreeWidget);

		q->clear();

		_controlledEntityID = entityID;

		auto& manager = avdecc::ControllerManager::getInstance();
		auto controlledEntity = manager.getControlledEntity(entityID);

		if (controlledEntity && node.getNode().has_value())
		{
			NodeVisitor::accept(this, controlledEntity.get(), node);
		}

		q->expandAll();
	}

private:
	virtual void visit(la::avdecc::controller::ControlledEntity const* const controlledEntity, la::avdecc::controller::model::EntityNode const& node) noexcept override
	{
		createIdItem(&node);
		createAccessItem(&node);

		Q_Q(NodeTreeWidget);

		// Names
		{
			auto* nameItem = new QTreeWidgetItem(q);
			nameItem->setText(0, "Names");
			
			addEditableTextItem(nameItem, "Entity Name", avdecc::helper::entityName(*controlledEntity), avdecc::ControllerManager::AecpCommandType::SetEntityName, {});
			addEditableTextItem(nameItem, "Group Name", avdecc::helper::groupName(*controlledEntity), avdecc::ControllerManager::AecpCommandType::SetEntityGroupName, {});
		}
		
		// Static model
		{
			auto* descriptorItem = new QTreeWidgetItem(q);
			descriptorItem->setText(0, "Static Info");
			
			auto const* const staticModel = node.staticModel;
			auto const* const dynamicModel = node.dynamicModel;
			
			// Currently, use the getEntity() information, but maybe in the future the controller will have the information in its static/dynamic model
			{
				auto const& entity = controlledEntity->getEntity();
				auto const entityCaps = entity.getEntityCapabilities();
				auto const talkerCaps = entity.getTalkerCapabilities();
				auto const listenerCaps = entity.getListenerCapabilities();
				auto const ctrlCaps = entity.getControllerCapabilities();

				addTextItem(descriptorItem, "Entity Model ID", avdecc::helper::uniqueIdentifierToString(entity.getEntityModelID()));
				addTextItem(descriptorItem, "Entity Capabilities", avdecc::helper::toHexQString(la::avdecc::to_integral(entityCaps), true, true) + QString(" (") + avdecc::helper::capabilitiesToString(entityCaps) + QString(")"));
				addTextItem(descriptorItem, "Talker Capabilities", avdecc::helper::toHexQString(la::avdecc::to_integral(talkerCaps), true, true) + QString(" (") + avdecc::helper::capabilitiesToString(talkerCaps) + QString(")"));
				addTextItem(descriptorItem, "Talker Sources", QString::number(entity.getTalkerStreamSources()));
				addTextItem(descriptorItem, "Listener Capabilities", avdecc::helper::toHexQString(la::avdecc::to_integral(listenerCaps), true, true) + QString(" (") + avdecc::helper::capabilitiesToString(listenerCaps) + QString(")"));
				addTextItem(descriptorItem, "Listener Sinks", QString::number(entity.getListenerStreamSinks()));
				addTextItem(descriptorItem, "Controller Capabilities", avdecc::helper::toHexQString(la::avdecc::to_integral(ctrlCaps), true, true) + QString(" (") + avdecc::helper::capabilitiesToString(ctrlCaps) + QString(")"));
			}
			
			addTextItem(descriptorItem, "Vendor Name", controlledEntity->getLocalizedString(staticModel->vendorNameString).data());
			addTextItem(descriptorItem, "Model Name", controlledEntity->getLocalizedString(staticModel->modelNameString).data());
			addTextItem(descriptorItem, "Firmware Version", dynamicModel->firmwareVersion.data());
			addTextItem(descriptorItem, "Serial Number", dynamicModel->serialNumber.data());

			addTextItem(descriptorItem, "Configuration Count", node.configurations.size());
		}

		// Discovery information
		{
#pragma message("TODO: Iterate over all discovered interfaces and print each info (when https://github.com/L-Acoustics/avdecc/issues/29 fixed)")
#pragma message("TODO: And listen for changes to dynamically update the info")
			createDiscoveryInfo(controlledEntity->getEntity());
		}
		
		// Dynamic model
		{
			auto* dynamicItem = new QTreeWidgetItem(q);
			dynamicItem->setText(0, "Dynamic Info");
			
			auto* currentConfigurationItem = new QTreeWidgetItem(dynamicItem);
			currentConfigurationItem->setText(0, "Current Configuration");
			
			auto* configurationComboBox = new qt::toolkit::ComboBox;
			
			for (auto const& it : node.configurations)
			{
				configurationComboBox->addItem(QString::number(it.first) + ": " + avdecc::helper::configurationName(controlledEntity, it.second), it.first);
			}
			
			auto currentConfigurationComboBoxIndex = configurationComboBox->findData(node.dynamicModel->currentConfiguration);
			q->setItemWidget(currentConfigurationItem, 1, configurationComboBox);
			
			// Send changes
			connect(configurationComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, configurationComboBox, node]()
			{
				auto const configurationIndex = configurationComboBox->currentData().value<la::avdecc::entity::model::ConfigurationIndex>();
				avdecc::ControllerManager::getInstance().setConfiguration(_controlledEntityID, configurationIndex);
			});
			
			// Initialize current value
			{
				QSignalBlocker const lg{ configurationComboBox }; // Block internal signals so setCurrentIndex do not trigger "currentIndexChanged"
				configurationComboBox->setCurrentIndex(currentConfigurationComboBoxIndex);
			}
		}
	}

	virtual void visit(la::avdecc::controller::ControlledEntity const* const controlledEntity, la::avdecc::controller::model::ConfigurationNode const& node) noexcept override
	{
		createIdItem(&node);
		createNameItem(controlledEntity, node, avdecc::ControllerManager::AecpCommandType::SetConfigurationName, node.descriptorIndex);
	}

	virtual void visit(la::avdecc::controller::ControlledEntity const* const controlledEntity, la::avdecc::controller::model::AudioUnitNode const& node) noexcept override
	{
		createIdItem(&node);
		createNameItem(controlledEntity, node, avdecc::ControllerManager::AecpCommandType::None, {}); // SetName not supported yet

		Q_Q(NodeTreeWidget);

		// Static model
		{
			auto* descriptorItem = new QTreeWidgetItem(q);
			descriptorItem->setText(0, "Static Info");

			auto const* const model = node.staticModel;

			addTextItem(descriptorItem, "Clock Domain Index", model->clockDomainIndex);
		}

		// Dynamic model
		{
			auto* dynamicItem = new AudioUnitDynamicTreeWidgetItem(_controlledEntityID, node.descriptorIndex, node.staticModel, node.dynamicModel, q);
			dynamicItem->setText(0, "Dynamic Info");
		}
	}

	virtual void visit(la::avdecc::controller::ControlledEntity const* const controlledEntity, la::avdecc::controller::model::StreamInputNode const& node) noexcept override
	{
		createIdItem(&node);
		createNameItem(controlledEntity, node, avdecc::ControllerManager::AecpCommandType::SetStreamName, std::make_tuple(controlledEntity->getEntityNode().dynamicModel->currentConfiguration, node.descriptorType, node.descriptorIndex));

		Q_Q(NodeTreeWidget);

		// Static model
		{
			auto* descriptorItem = new QTreeWidgetItem(q);
			descriptorItem->setText(0, "Static Info");

			auto const* const model = node.staticModel;

			addTextItem(descriptorItem, "AVB Interface Index", model->avbInterfaceIndex);
			addTextItem(descriptorItem, "Clock Domain Index", model->clockDomainIndex);
		}

		// Dynamic model
		{
			auto* dynamicItem = new StreamDynamicTreeWidgetItem(_controlledEntityID, node.descriptorType, node.descriptorIndex, node.staticModel, node.dynamicModel, nullptr, q);
			dynamicItem->setText(0, "Dynamic Info");
		}
	}

	virtual void visit(la::avdecc::controller::ControlledEntity const* const controlledEntity, la::avdecc::controller::model::StreamOutputNode const& node) noexcept override
	{
		createIdItem(&node);
		createNameItem(controlledEntity, node, avdecc::ControllerManager::AecpCommandType::SetStreamName, std::make_tuple(controlledEntity->getEntityNode().dynamicModel->currentConfiguration, node.descriptorType, node.descriptorIndex));

		Q_Q(NodeTreeWidget);

		// Static model
		{
			auto* descriptorItem = new QTreeWidgetItem(q);
			descriptorItem->setText(0, "Static Info");

			auto const* const model = node.staticModel;

			addTextItem(descriptorItem, "AVB Interface Index", model->avbInterfaceIndex);
			addTextItem(descriptorItem, "Clock Domain Index", model->clockDomainIndex);
		}

		// Dynamic model
		{
			auto* dynamicItem = new StreamDynamicTreeWidgetItem(_controlledEntityID, node.descriptorType, node.descriptorIndex, node.staticModel, nullptr, node.dynamicModel, q);
			dynamicItem->setText(0, "Dynamic Info");
		}
	}

	virtual void visit(la::avdecc::controller::ControlledEntity const* const controlledEntity, la::avdecc::controller::model::AvbInterfaceNode const& node) noexcept override
	{
		createIdItem(&node);
		createNameItem(controlledEntity, node, avdecc::ControllerManager::AecpCommandType::None, {}); // SetName not supported yet

		Q_Q(NodeTreeWidget);

		// Static model
		{
			auto* descriptorItem = new QTreeWidgetItem(q);
			descriptorItem->setText(0, "Static Info");

			auto const* const model = node.staticModel;

			addTextItem(descriptorItem, "MAC Address", la::avdecc::networkInterface::macAddressToString(model->macAddress, true));
			addTextItem(descriptorItem, "Flags", avdecc::helper::toHexQString(la::avdecc::to_integral(model->interfaceFlags), true, true) + QString(" (") + avdecc::helper::flagsToString(model->interfaceFlags) + QString(")"));
			addTextItem(descriptorItem, "Clock Identity", avdecc::helper::uniqueIdentifierToString(model->clockIdentity));
			addTextItem(descriptorItem, "Priority 1", avdecc::helper::toHexQString(model->priority1, true, true));
			addTextItem(descriptorItem, "Clock Class", avdecc::helper::toHexQString(model->clockClass, true, true));
			addTextItem(descriptorItem, "Offset Scaled Log Variance", avdecc::helper::toHexQString(model->offsetScaledLogVariance, true, true));
			addTextItem(descriptorItem, "Clock Accuracy", avdecc::helper::toHexQString(model->clockAccuracy, true, true));
			addTextItem(descriptorItem, "Priority 2", avdecc::helper::toHexQString(model->priority2, true, true));
			addTextItem(descriptorItem, "Domain Number", avdecc::helper::toHexQString(model->domainNumber, true, true));
			addTextItem(descriptorItem, "Log Sync Interval", avdecc::helper::toHexQString(model->logSyncInterval, true, true));
			addTextItem(descriptorItem, "Log Announce Interval", avdecc::helper::toHexQString(model->logAnnounceInterval, true, true));
			addTextItem(descriptorItem, "Log Delay Interval", avdecc::helper::toHexQString(model->logPDelayInterval, true, true));
			addTextItem(descriptorItem, "Port Number", avdecc::helper::toHexQString(model->portNumber, true, true));
		}

		// Dynamic model
		{
			auto* dynamicItem = new AvbInterfaceDynamicTreeWidgetItem(_controlledEntityID, node.descriptorIndex, node.dynamicModel, q);
			dynamicItem->setText(0, "Dynamic Info");
		}
	}

	virtual void visit(la::avdecc::controller::ControlledEntity const* const controlledEntity, la::avdecc::controller::model::ClockSourceNode const& node) noexcept override
	{
		createIdItem(&node);
		createNameItem(controlledEntity, node, avdecc::ControllerManager::AecpCommandType::None, {}); // SetName not supported yet

		Q_Q(NodeTreeWidget);

		// Static model (and dynamic read-only part)
		{
			auto* descriptorItem = new QTreeWidgetItem(q);
			descriptorItem->setText(0, "Static Info");

			auto const* const model = node.staticModel;
			auto const* const dynamicModel = node.dynamicModel;

			addTextItem(descriptorItem, "Clock Source Type", avdecc::helper::clockSourceTypeToString(model->clockSourceType));
			addTextItem(descriptorItem, "Flags", avdecc::helper::toHexQString(la::avdecc::to_integral(dynamicModel->clockSourceFlags), true, true) + QString(" (") + avdecc::helper::flagsToString(dynamicModel->clockSourceFlags) + QString(")"));

			addTextItem(descriptorItem, "Clock Source Identifier", avdecc::helper::uniqueIdentifierToString(dynamicModel->clockSourceIdentifier));
			addTextItem(descriptorItem, "Clock Source Location Type", avdecc::helper::descriptorTypeToString(model->clockSourceLocationType));
			addTextItem(descriptorItem, "Clock Source Location Index", model->clockSourceLocationIndex);
		}
	}

	virtual void visit(la::avdecc::controller::ControlledEntity const* const controlledEntity, la::avdecc::controller::model::LocaleNode const& node) noexcept override
	{
		createIdItem(&node);

		Q_Q(NodeTreeWidget);

		// Static model
		{
			auto* descriptorItem = new QTreeWidgetItem(q);
			descriptorItem->setText(0, "Static Info");

			auto const* const model = node.staticModel;

			addTextItem(descriptorItem, "Number of String Descriptors", model->numberOfStringDescriptors);
		}
	}

	virtual void visit(la::avdecc::controller::ControlledEntity const* const controlledEntity, la::avdecc::controller::model::StringsNode const& node) noexcept override
	{
		createIdItem(&node);
	}

	virtual void visit(la::avdecc::controller::ControlledEntity const* const controlledEntity, la::avdecc::controller::model::StreamPortNode const& node) noexcept override
	{
		createIdItem(&node);

		Q_Q(NodeTreeWidget);

		// Static model
		{
			auto* descriptorItem = new QTreeWidgetItem(q);
			descriptorItem->setText(0, "Static Info");

			auto const* const model = node.staticModel;

			addTextItem(descriptorItem, "Clock Domain Index", model->clockDomainIndex);
			addTextItem(descriptorItem, "Flags", avdecc::helper::toHexQString(la::avdecc::to_integral(model->portFlags), true, true) + QString(" (") + avdecc::helper::flagsToString(model->portFlags) + QString(")"));
			addTextItem(descriptorItem, "Supports Dynamic Mapping", model->hasDynamicAudioMap ? "Yes" : "No");
		}

		// Dynamic model
		if (node.staticModel->hasDynamicAudioMap)
		{
			auto* dynamicItem = new StreamPortDynamicTreeWidgetItem(_controlledEntityID, node.descriptorType, node.descriptorIndex, node.staticModel, node.dynamicModel, q);
			dynamicItem->setText(0, "Dynamic Info");
		}
	}

	virtual void visit(la::avdecc::controller::ControlledEntity const* const controlledEntity, la::avdecc::controller::model::AudioClusterNode const& node) noexcept override
	{
		createIdItem(&node);
		createNameItem(controlledEntity, node, avdecc::ControllerManager::AecpCommandType::None, {}); // SetName not supported yet

		Q_Q(NodeTreeWidget);

		// Static model
		{
			auto* descriptorItem = new QTreeWidgetItem(q);
			descriptorItem->setText(0, "Static Info");

			auto const* const model = node.staticModel;

			addTextItem(descriptorItem, "Signal Type", avdecc::helper::descriptorTypeToString(model->signalType));
			addTextItem(descriptorItem, "Signal Index", model->signalIndex);

			addTextItem(descriptorItem, "Signal Output", model->signalOutput);
			addTextItem(descriptorItem, "Path Latency", model->pathLatency);
			addTextItem(descriptorItem, "Block Latency", model->blockLatency);
			addTextItem(descriptorItem, "channel Count", model->channelCount);
			addTextItem(descriptorItem, "Format", avdecc::helper::audioClusterFormatToString(model->format));
		}
	}

	virtual void visit(la::avdecc::controller::ControlledEntity const* const controlledEntity, la::avdecc::controller::model::AudioMapNode const& node) noexcept override
	{
		createIdItem(&node);

		Q_Q(NodeTreeWidget);

		// Static model
		{
			auto* descriptorItem = new QTreeWidgetItem(q);
			descriptorItem->setText(0, "Static Info");

			auto const* const model = node.staticModel;

			auto* mappingsIndexItem = new QTreeWidgetItem(descriptorItem);
			mappingsIndexItem->setText(0, "Mappings");

			auto* listWidget = new QListWidget;

			q->setItemWidget(mappingsIndexItem, 1, listWidget);
			listWidget->setStyleSheet(".QListWidget{margin-top:4px;margin-bottom:4px}");

			for (auto const& mapping : model->mappings)
			{
				listWidget->addItem(QString("%1.%2 > %3.%4").arg(mapping.streamIndex).arg(mapping.streamChannel).arg(mapping.clusterOffset).arg(mapping.clusterChannel));
			}
		}
	}

	virtual void visit(la::avdecc::controller::ControlledEntity const* const controlledEntity, la::avdecc::controller::model::ClockDomainNode const& node) noexcept override
	{
		createIdItem(&node);
		createNameItem(controlledEntity, node, avdecc::ControllerManager::AecpCommandType::None, {}); // SetName not supported yet

		Q_Q(NodeTreeWidget);
		auto const* const model = node.staticModel;
		auto const* const dynamicModel = node.dynamicModel;

		// Static model
		{
			auto* descriptorItem = new QTreeWidgetItem(q);
			descriptorItem->setText(0, "Static Info");

			addTextItem(descriptorItem, "Clock Sources count", model->clockSources.size());
		}
		
		// Dynamic model
		{
			auto* dynamicItem = new QTreeWidgetItem(q);
			dynamicItem->setText(0, "Dynamic Info");

			auto* currentSourceItem = new QTreeWidgetItem(dynamicItem);
			currentSourceItem->setText(0, "Current Clock Source");
			
			auto* sourceComboBox = new qt::toolkit::ComboBox;
			
			for (auto const sourceIndex : model->clockSources)
			{
				try
				{
					auto const& node = controlledEntity->getClockSourceNode(controlledEntity->getEntityNode().dynamicModel->currentConfiguration, sourceIndex);
					auto const name = QString::number(sourceIndex) + ": '" + avdecc::helper::objectName(controlledEntity, node) + "' (" + avdecc::helper::clockSourceToString(node) + ")";
					sourceComboBox->addItem(name, QVariant::fromValue(sourceIndex));
				}
				catch (...)
				{
				}
			}
			
			auto const currentSourceComboBoxIndex = sourceComboBox->findData(dynamicModel->clockSourceIndex);
			q->setItemWidget(currentSourceItem, 1, sourceComboBox);
			
			// Send changes
			connect(sourceComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, sourceComboBox, node]()
			{
				auto const clockDomainIndex = node.descriptorIndex;
				auto const sourceIndex = sourceComboBox->currentData().value<la::avdecc::entity::model::ClockSourceIndex>();
				avdecc::ControllerManager::getInstance().setClockSource(_controlledEntityID, clockDomainIndex, sourceIndex);
			});
			
			// Listen for changes
			connect(&avdecc::ControllerManager::getInstance(), &avdecc::ControllerManager::clockSourceChanged, sourceComboBox, [this, streamType = node.descriptorType, domainIndex = node.descriptorIndex, sourceComboBox](la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::ClockDomainIndex const clockDomainIndex, la::avdecc::entity::model::ClockSourceIndex const sourceIndex)
			{
				if (entityID == _controlledEntityID && clockDomainIndex == domainIndex)
				{
					auto index = sourceComboBox->findData(QVariant::fromValue(sourceIndex));
					AVDECC_ASSERT(index != -1, "Index not found");
					if (index != -1)
					{
						QSignalBlocker const lg{ sourceComboBox }; // Block internal signals so setCurrentIndex do not trigger "currentIndexChanged"
						sourceComboBox->setCurrentIndex(index);
					}
				}
			});
			
			// Initialize current value
			{
				QSignalBlocker const lg{ sourceComboBox }; // Block internal signals so setCurrentIndex do not trigger "currentIndexChanged"
				sourceComboBox->setCurrentIndex(currentSourceComboBoxIndex);
			}
		}
	}

	virtual void visit(la::avdecc::controller::ControlledEntity const* const controlledEntity, la::avdecc::controller::model::RedundantStreamNode const& node) noexcept override
	{
		//createIdItem(&node);
		//createNameItem(controlledEntity, node.clockDomainDescriptor, avdecc::ControllerManager::CommandType::None, {}); // SetName not supported yet
	}

	virtual void visit(la::avdecc::controller::ControlledEntity const* const controlledEntity, la::avdecc::controller::model::MemoryObjectNode const& node) noexcept override
	{
		createIdItem(&node);
		createNameItem(controlledEntity, node, avdecc::ControllerManager::AecpCommandType::None, {}); // SetName not supported yet

		Q_Q(NodeTreeWidget);

		// Static model
		{
			auto* descriptorItem = new QTreeWidgetItem(q);
			descriptorItem->setText(0, "Static Info");

			auto const* const model = node.staticModel;
			addTextItem(descriptorItem, "Memory object type", avdecc::helper::memoryObjectTypeToString(model->memoryObjectType));
			addTextItem(descriptorItem, "Target descriptor type", avdecc::helper::descriptorTypeToString(model->targetDescriptorType));
			addTextItem(descriptorItem, "Target descriptor index", model->targetDescriptorIndex);
			addTextItem(descriptorItem, "Start address", avdecc::helper::toHexQString(model->startAddress, false, true));
			addTextItem(descriptorItem, "Maximum length", avdecc::helper::toHexQString(model->maximumLength, false, true));
			
			addImageItem(descriptorItem, "Preview", model->memoryObjectType);
		}

		// Dynamic model
		{
			auto* dynamicItem = new MemoryObjectDynamicTreeWidgetItem(_controlledEntityID, node.descriptorIndex, node.dynamicModel, q);
			dynamicItem->setText(0, "Dynamic Info");
		}
	}

private:
	QTreeWidgetItem * createIdItem(la::avdecc::controller::model::EntityModelNode const* node)
	{
		Q_Q(NodeTreeWidget);

		auto* idItem = new QTreeWidgetItem(q);
		idItem->setText(0, "Id");

		auto* descriptorTypeItem = new QTreeWidgetItem(idItem);
		descriptorTypeItem->setText(0, "Descriptor Type");
		descriptorTypeItem->setText(1, avdecc::helper::descriptorTypeToString(node->descriptorType));

		auto* descriptorIndexItem = new QTreeWidgetItem(idItem);
		descriptorIndexItem->setText(0, "Descriptor Index");
		descriptorIndexItem->setText(1, QString::number(node->descriptorIndex));

		return idItem;
	}

	QTreeWidgetItem* createAccessItem(la::avdecc::controller::model::EntityModelNode const* node)
	{
		Q_Q(NodeTreeWidget);

		auto* accessItem = new QTreeWidgetItem(q);
		accessItem->setText(0, "Exclusive Access");

		auto* acquireStateItem = new QTreeWidgetItem(accessItem);
		acquireStateItem->setText(0, "Acquire State");
		acquireStateItem->setText(1, avdecc::helper::acquireStateToString(node->acquireState));

		return accessItem;
	}
	
	QTreeWidgetItem* createDiscoveryInfo(la::avdecc::entity::Entity const& entity)
	{
		Q_Q(NodeTreeWidget);
		auto const entityCaps = entity.getEntityCapabilities();

		auto* discoveryItem = new QTreeWidgetItem(q);
		if (la::avdecc::hasFlag(entityCaps, la::avdecc::entity::EntityCapabilities::AemInterfaceIndexValid))
		{
			discoveryItem->setText(0, QString("Interface Index %1").arg(entity.getInterfaceIndex()));
		}
		else
		{
			discoveryItem->setText(0, QString("Interface (Index Not Set)"));
		}

		addTextItem(discoveryItem, "MAC Address", la::avdecc::networkInterface::macAddressToString(entity.getMacAddress(), true));
		addTextItem(discoveryItem, "Entity Capabilities", avdecc::helper::toHexQString(la::avdecc::to_integral(entityCaps), true, true) + QString(" (") + avdecc::helper::capabilitiesToString(entityCaps) + QString(")"));
		addTextItem(discoveryItem, "Grandmaster ID", avdecc::helper::uniqueIdentifierToString(entity.getGptpGrandmasterID()));
		addTextItem(discoveryItem, "Grandmaster Domain Number", QString::number(entity.getGptpDomainNumber()));
		addTextItem(discoveryItem, "Association ID", la::avdecc::hasFlag(entityCaps, la::avdecc::entity::EntityCapabilities::AssociationIDValid) ? avdecc::helper::uniqueIdentifierToString(entity.getAssociationID()) : QString("Not Set"));
		addTextItem(discoveryItem, "Identify Control Index", la::avdecc::hasFlag(entityCaps, la::avdecc::entity::EntityCapabilities::AemIdentifyControlIndexValid) ? QString::number(entity.getIdentifyControlIndex()) : QString("Not Set"));

		return discoveryItem;
	}

	template<class NodeType>
	QTreeWidgetItem* createNameItem(la::avdecc::controller::ControlledEntity const* const controlledEntity, NodeType const& node, avdecc::ControllerManager::AecpCommandType commandType, std::any const& customData)
	{
		Q_Q(NodeTreeWidget);

		auto* nameItem = new QTreeWidgetItem(q);
		nameItem->setText(0, "Name");

		if (commandType != avdecc::ControllerManager::AecpCommandType::None)
		{
			addEditableTextItem(nameItem, "Name", node.dynamicModel->objectName.data(), commandType, customData);
		}
		else
		{
			addTextItem(nameItem, "Name", node.dynamicModel->objectName.data());
		}

		auto* localizedNameItem = new QTreeWidgetItem(nameItem);
		localizedNameItem->setText(0, "Localized Name");
		localizedNameItem->setText(1, controlledEntity->getLocalizedString(node.staticModel->localizedDescription).data());

		return nameItem;
	}

	/** A label (readonly) item */
	template<typename ValueType>
	void addTextItem(QTreeWidgetItem* const treeWidgetItem, QString itemName, ValueType itemValue)
	{
		auto* item = new QTreeWidgetItem(treeWidgetItem);
		item->setText(0, std::move(itemName));
		item->setData(1, Qt::DisplayRole, std::move(itemValue));
	}
	void addTextItem(QTreeWidgetItem* const treeWidgetItem, QString itemName, QString itemValue)
	{
		auto* item = new QTreeWidgetItem(treeWidgetItem);
		item->setText(0, std::move(itemName));
		item->setText(1, std::move(itemValue));
	}
	void addTextItem(QTreeWidgetItem* const treeWidgetItem, QString itemName, std::string const& itemValue)
	{
		addTextItem(treeWidgetItem, std::move(itemName), QString::fromStdString(itemValue));
	}
	void addTextItem(QTreeWidgetItem* const treeWidgetItem, QString itemName, unsigned long const& itemValue)
	{
		addTextItem(treeWidgetItem, std::move(itemName), QVariant::fromValue(itemValue));
	}

	/** An editable text entry item */
	void addEditableTextItem(QTreeWidgetItem* const treeWidgetItem, QString itemName, QString itemValue, avdecc::ControllerManager::AecpCommandType commandType, std::any const& customData)
	{
		Q_Q(NodeTreeWidget);

		auto* item = new QTreeWidgetItem(treeWidgetItem);
		item->setText(0, itemName);

		auto* textEntry = new qt::toolkit::TextEntry(itemValue);

		q->setItemWidget(item, 1, textEntry);

		connect(&avdecc::ControllerManager::getInstance(), &avdecc::ControllerManager::beginAecpCommand, textEntry, [this, commandType, textEntry](la::avdecc::UniqueIdentifier const entityID, avdecc::ControllerManager::AecpCommandType cmdType)
		{
			if (entityID == _controlledEntityID && cmdType == commandType)
				textEntry->setEnabled(false);
		});

		connect(textEntry, &qt::toolkit::TextEntry::returnPressed, textEntry, [this, textEntry, commandType, customData]()
		{
			// Send changes
			switch (commandType)
			{
				case avdecc::ControllerManager::AecpCommandType::SetEntityName:
					avdecc::ControllerManager::getInstance().setEntityName(_controlledEntityID, textEntry->text());
					break;
				case avdecc::ControllerManager::AecpCommandType::SetEntityGroupName:
					avdecc::ControllerManager::getInstance().setEntityGroupName(_controlledEntityID, textEntry->text());
					break;
				case avdecc::ControllerManager::AecpCommandType::SetConfigurationName:
					try
					{
						auto const configIndex = std::any_cast<la::avdecc::entity::model::ConfigurationIndex>(customData);
						avdecc::ControllerManager::getInstance().setConfigurationName(_controlledEntityID, configIndex, textEntry->text());
					}
					catch (...)
					{
					}
					break;
				case avdecc::ControllerManager::AecpCommandType::SetStreamName:
					try
					{
						auto const customTuple = std::any_cast<std::tuple<la::avdecc::entity::model::ConfigurationIndex, la::avdecc::entity::model::DescriptorType, la::avdecc::entity::model::StreamIndex>>(customData);
						auto const configIndex = std::get<0>(customTuple);
						auto const streamType = std::get<1>(customTuple);
						auto const streamIndex = std::get<2>(customTuple);
						if (streamType == la::avdecc::entity::model::DescriptorType::StreamInput)
							avdecc::ControllerManager::getInstance().setStreamInputName(_controlledEntityID, configIndex, streamIndex, textEntry->text());
						else if (streamType == la::avdecc::entity::model::DescriptorType::StreamOutput)
							avdecc::ControllerManager::getInstance().setStreamOutputName(_controlledEntityID, configIndex, streamIndex, textEntry->text());
					}
					catch (...)
					{
					}
					break;
				default:
					break;
			}
		});

		connect(&avdecc::ControllerManager::getInstance(), &avdecc::ControllerManager::endAecpCommand, textEntry, [this, commandType, textEntry](la::avdecc::UniqueIdentifier const entityID, avdecc::ControllerManager::AecpCommandType cmdType, la::avdecc::entity::ControllerEntity::AemCommandStatus const status)
		{
			if (entityID == _controlledEntityID && cmdType == commandType)
				textEntry->setEnabled(true);
		});

		// Listen for changes
		try
		{
			switch (commandType)
			{
				case avdecc::ControllerManager::AecpCommandType::SetEntityName:
					connect(&avdecc::ControllerManager::getInstance(), &avdecc::ControllerManager::entityNameChanged, textEntry, [this, textEntry](la::avdecc::UniqueIdentifier const entityID, QString const& entityName)
					{
						if (entityID == _controlledEntityID)
							textEntry->setText(entityName);
					});
					break;
				case avdecc::ControllerManager::AecpCommandType::SetEntityGroupName:
					connect(&avdecc::ControllerManager::getInstance(), &avdecc::ControllerManager::entityGroupNameChanged, textEntry, [this, textEntry](la::avdecc::UniqueIdentifier const entityID, QString const& entityGroupName)
					{
						if (entityID == _controlledEntityID)
							textEntry->setText(entityGroupName);
					});
					break;
				case avdecc::ControllerManager::AecpCommandType::SetConfigurationName:
				{
					auto const configIndex = std::any_cast<la::avdecc::entity::model::ConfigurationIndex>(customData);
					connect(&avdecc::ControllerManager::getInstance(), &avdecc::ControllerManager::configurationNameChanged, textEntry, [this, textEntry, configIndex](la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, QString const& configurationName)
					{
						if (entityID == _controlledEntityID && configurationIndex == configIndex)
							textEntry->setText(configurationName);
					});
					break;
				}
				case avdecc::ControllerManager::AecpCommandType::SetStreamName:
				{
					auto const customTuple = std::any_cast<std::tuple<la::avdecc::entity::model::ConfigurationIndex, la::avdecc::entity::model::DescriptorType, la::avdecc::entity::model::StreamIndex>>(customData);
					auto const configIndex = std::get<0>(customTuple);
					auto const streamType = std::get<1>(customTuple);
					auto const streamIndex = std::get<2>(customTuple);
					connect(&avdecc::ControllerManager::getInstance(), &avdecc::ControllerManager::streamNameChanged, textEntry, [this, textEntry, configIndex, streamType, strIndex = streamIndex](la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::DescriptorType const descriptorType, la::avdecc::entity::model::StreamIndex const streamIndex, QString const& streamName)
					{
						if (entityID == _controlledEntityID && configurationIndex == configIndex && descriptorType == streamType && streamIndex == strIndex)
							textEntry->setText(streamName);
					});
					break;
				}
				default:
					break;
			}
		}
		catch (...)
		{
		}
	}
	
	void addImageItem(QTreeWidgetItem* const treeWidgetItem, QString itemName, la::avdecc::entity::model::MemoryObjectType const memoryObjectType)
	{
		auto type = EntityLogoCache::Type::None;
		switch (memoryObjectType)
		{
			case la::avdecc::entity::model::MemoryObjectType::PngEntity:
				type = EntityLogoCache::Type::Entity;
				break;
			case la::avdecc::entity::model::MemoryObjectType::PngManufacturer:
				type = EntityLogoCache::Type::Manufacturer;
				break;
			default:
				return;
		}
		
		auto const image = EntityLogoCache::getInstance().getImage(_controlledEntityID, type);

		Q_Q(NodeTreeWidget);
		
		auto* item = new QTreeWidgetItem(treeWidgetItem);
		item->setText(0, std::move(itemName));
		
		auto* label = new Label;
		label->setFixedHeight(96);
		label->setImage(image);
		q->setItemWidget(item, 1, label);
		
		connect(label, &Label::clicked, label, [this, requestedType = type]()
		{
			EntityLogoCache::getInstance().getImage(_controlledEntityID, requestedType, true);
		});
		
		connect(&EntityLogoCache::getInstance(), &EntityLogoCache::imageChanged, label, [this, label, requestedType = type](const la::avdecc::UniqueIdentifier entityID, const EntityLogoCache::Type type)
		{
			if (entityID == _controlledEntityID && type == requestedType)
			{
				auto const image = EntityLogoCache::getInstance().getImage(_controlledEntityID, type);
				label->setImage(image);
			}
		});
	}

private:
	NodeTreeWidget * const q_ptr{ nullptr };
	Q_DECLARE_PUBLIC(NodeTreeWidget);

	la::avdecc::UniqueIdentifier _controlledEntityID{};
};

NodeTreeWidget::NodeTreeWidget(QWidget* parent)
	: QTreeWidget(parent)
	, d_ptr(new NodeTreeWidgetPrivate(this))
{
	setSelectionBehavior(QAbstractItemView::SelectRows);
	setSelectionMode(QAbstractItemView::SingleSelection);
	header()->resizeSection(0, 200);
}

NodeTreeWidget::~NodeTreeWidget()
{
	delete d_ptr;
}

void NodeTreeWidget::setNode(la::avdecc::UniqueIdentifier const entityID, AnyNode const& node)
{
	Q_D(NodeTreeWidget);
	d->setNode(entityID, node);
}

#include "nodeTreeWidget.moc"

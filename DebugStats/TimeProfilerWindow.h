// For conditions of distribution and use, see copyright notice in license.txt

#ifndef incl_DebugStats_TimeProfilerWindow_h
#define incl_DebugStats_TimeProfilerWindow_h

#include <QTreeWidget>
#include <QTimer>
#include <QComboBox>
#include <QTabWidget>
#include <QLabel>
#include <QTreeWidget>
#include <QPushButton>

#include "Framework.h"
#include "UiModule.h"
#include "WorldStream.h"
#include "NetworkMessages/NetInMessage.h"
#include <boost/cstdint.hpp>

class TimeProfilerWindow : public QWidget
{
    Q_OBJECT

    Foundation::Framework *framework_;

    QTreeWidget *tree_profiling_data_;
    QComboBox *combo_timing_refresh_interval_;
    QTabWidget *tab_widget_;
    QWidget *contents_widget_;
    QLabel *label_frame_time_history_;
    QLabel *label_top_frame_time_;
    QLabel *label_time_per_frame_;
    QLabel *label_region_map_coords_;
    QLabel *label_region_object_capacity_;
    QLabel *label_pid_stat_;
    QTreeWidget *tree_sim_stats_;
    QPushButton *push_button_toggle_tree_;
    QPushButton *push_button_collapse_all_;
    QPushButton *push_button_expand_all_;
    QPushButton *push_button_show_unused_;

    int frame_time_update_x_pos_;

    // If true, profiling data is shown in a tree, otherwise using a flat list.
    bool show_profiler_tree_;

    bool show_unused_;

    QTimer profiler_update_timer_;

    ProtocolUtilities::WorldStreamPtr current_world_stream_;

    void FillProfileTimingWindow(QTreeWidgetItem *qtNode, const Foundation::ProfilerNodeTree *profilerNode);

    int ReadProfilingRefreshInterval();
    void RefreshProfilingDataTree();
    void RefreshProfilingDataList();
    void CollectProfilerNodes(Foundation::ProfilerNodeTree *node, std::vector<const Foundation::ProfilerNode *> &dst);
    void resizeEvent(QResizeEvent *event);

public:
    /// The ctor adds this window to scene, but does not show it.
    explicit TimeProfilerWindow(UiServices::UiModule *uiModule, Foundation::Framework *framework);
    void RedrawFrameTimeHistoryGraph(const std::vector<std::pair<boost::uint64_t, double> > &frameTimes);
    void RedrawFrameTimeHistoryGraphDelta(const std::vector<std::pair<boost::uint64_t, double> > &frameTimes);
    void SetWorldStreamPtr(ProtocolUtilities::WorldStreamPtr worldStream);
    void RefreshSimStatsData(ProtocolUtilities::NetInMessage *simStats);

public slots:
    void RefreshProfilingData();
    void OnProfilerWindowTabChanged(int newPage);
    void RefreshOgreProfilingWindow();
    void RefreshNetworkProfilingData();
    void ToggleTreeButtonPressed();
    void CollapseAllButtonPressed();
    void ExpandAllButtonPressed();
    void show_unused_ButtonPressed();
};

#endif

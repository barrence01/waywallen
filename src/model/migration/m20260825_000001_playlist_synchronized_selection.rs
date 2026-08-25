use sea_orm_migration::prelude::*;

#[derive(DeriveMigrationName)]
pub struct Migration;

#[async_trait::async_trait]
impl MigrationTrait for Migration {
    async fn up(&self, manager: &SchemaManager) -> Result<(), DbErr> {
        manager
            .alter_table(
                Table::alter()
                    .table(Playlist::Table)
                    .add_column(
                        ColumnDef::new(Playlist::SynchronizedSelection)
                            .boolean()
                            .not_null()
                            .default(true),
                    )
                    .to_owned(),
            )
            .await
    }

    async fn down(&self, manager: &SchemaManager) -> Result<(), DbErr> {
        manager
            .alter_table(
                Table::alter()
                    .table(Playlist::Table)
                    .drop_column(Playlist::SynchronizedSelection)
                    .to_owned(),
            )
            .await
    }
}

#[derive(DeriveIden)]
enum Playlist {
    Table,
    SynchronizedSelection,
}
